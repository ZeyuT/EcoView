# Zeyu Tang
# Clemson University
# Eco-coating project
# 02/2025
# This script contains several functions for generating videos from heigh maps measured over time

import sys
import random
import numpy as np
import pandas
import os
import glob
import pickle
import cv2
from pystackreg import StackReg
from skimage.filters import gaussian

# Example usage with dummy data (replace with actual frame data)
# frames = [cv2.imread('frame1.png'), cv2.imread('frame2.png'), ...]
# stabilized_frames = stabilize_frames(frames, StackReg.RIGID_BODY)

def find_longest_good_segment(file_list, good_files, max_bad_frames):
    """
    Finds the longest sequence of good frames, interpolating over short gaps of bad frames.

    Args:
        file_list: the list of frame file paths.
        good_files: dictionary with pairs of [filename, 1 or 0]. 1: good; 0: bad
        max_bad_frames: maximum number of consecutive bad frames allowed for replacement with interpolation.

    Returns:
        longest_segment: The longest sequence of frames (numpy arrays) after interpolation.
        frame_names: list of file names corrsponds to longest_segment
    """
    longest_segment = []
    cur_segment = []
    longest_filenames = []
    cur_filenames = []
    bad_count = 0
    in_good_sequence = False  # Flag to indicate if we are currently in a good sequence

    for i, file_path in enumerate(file_list):
        filename = os.path.splitext(os.path.basename(file_path))[0]
        
        if filename not in good_files:
            raise ValueError(f"Filename '{filename}' not found in good_files dictionary.")

        if good_files[filename]:
            if bad_count > 0 and bad_count <= max_bad_frames and in_good_sequence:
                start_frame = cv2.imread(file_list[i - bad_count - 1], cv2.IMREAD_UNCHANGED)
                end_frame = cv2.imread(file_path, cv2.IMREAD_UNCHANGED)
                for j in range(1, bad_count + 1):
                    t = j / (bad_count + 1)
                    interpolated_frame = cv2.addWeighted(start_frame, 1 - t, end_frame, t, 0)
                    cur_segment.append(interpolated_frame)
                    cur_filenames.append('interpolated')
            cur_frame = cv2.imread(file_path, cv2.IMREAD_UNCHANGED)
            cur_segment.append(cur_frame)
            cur_filenames.append(filename)
            bad_count = 0
            in_good_sequence = True  # We are in a valid sequence
        else:
            if in_good_sequence:
                bad_count += 1
            else:
                continue  # Ignore bad frames until we find the first good frame

        if bad_count > max_bad_frames:
            # End the current segment if too many consecutive bad frames
            if len(cur_segment) > len(longest_segment):
                longest_segment = cur_segment
                longest_filenames = cur_filenames
            cur_segment = []
            cur_filenames = []
            bad_count = 0
            in_good_sequence = False  # Reset the sequence flag

    # Check the last segment after the loop
    if len(cur_segment) > len(longest_segment):
        longest_segment = cur_segment
        longest_filenames = cur_filenames

    return longest_segment, longest_filenames


def create_video(frames, output_path, fourcc, is_color=True, num_transitions=1, fps=24):
    """
    Creates a video from a list of grayscale frames, and save it to file

    Args:
        frames: a array with size [n_frames, H, W] frames to be included in the video.
        output_path (str): Path to save the output video file.
        fourcc: codec for writing videos.
        is_color: boolean value, set to True save the video with BGR channels, False to save the video in grayscale
        num_transitions: number of transition frames to be added between every two frames
        fps (int): Frame rate of the output video.

    Returns:
    - None
    """
    # Create VideoWriter object
    if len(frames):
        height, width = frames[0].shape[:2]
        if is_color and len(frames[0].shape) == 2:
            frames = [cv2.cvtColor(img, cv2.COLOR_GRAY2BGR) for img in frames]
        if not is_color and len(frames[0].shape) == 3:
            frames = [cv2.cvtColor(img, cv2.COLOR_BGR2GRAY) for img in frames]

        fourcc = cv2.VideoWriter_fourcc(*'mp4v')
        video = cv2.VideoWriter(output_path, fourcc, fps, (width, height), isColor=is_color)
        
        video.write(frames[0])
        # Loop through all frames, interpolate between them and add to the video
        for i in range(1, len(frames)):
            # Generate and add interpolated frames
            for transition_frame in linear_interpolation(frames[i-1], frames[i], num_transitions):
                video.write(transition_frame)
            # Add the actual next frame
            video.write(frames[i])
    
        video.release()
        print(f"Video saved as {output_path}")
    else:
        raise ValueError("No valid segment found for video generation.")

        
def linear_interpolation(first_frame, second_frame, num_interpolations):
    """
    Generate transition frames between two images using linear interpolation

    Args:
        first_frame: the first image frame (grayscale).
        second_frame: the second image frame (grayscale).
        num_interpolations: number of intermediate frames to generate.

    Returns:
        list of numpy.ndarray: a list containing the interpolated frames.
    """
    transition_frames = []
    for i in range(1, num_interpolations + 1):
        t = i / (num_interpolations + 1)
        interpolated_frame = cv2.addWeighted(first_frame, 1 - t, second_frame, t, 0)
        transition_frames.append(interpolated_frame)
    return transition_frames

def optical_flow_interpolation(first_frame, second_frame, num_interpolations):
    """
    Generate interpolated frames between two images using dense optical flow and sine easing for smooth transitions.
    Args:
        first_frame: the first image frame (grayscale).
        second_frame: the second image frame (grayscale).
        num_interpolations: number of intermediate frames to generate.

    Returns:
    - list of numpy.ndarray: A list containing the interpolated frames.
    """
    # Calculate dense optical flow
    flow = cv2.calcOpticalFlowFarneback(first_frame, second_frame, None, 0.5, 3, 15, 3, 5, 1.2, 0)

    height, width = first_frame.shape[:2]
    grid_x, grid_y = np.meshgrid(np.arange(width), np.arange(height), indexing='xy')

    transition_frames = []
    for i in range(1, num_interpolations + 1):
        t = i / (num_interpolations + 1)
        # Apply easing function
        eased_t = -0.5 * (np.cos(np.pi * t) - 1)
        # Interpolating the flow for the current position
        map_x = grid_x + flow[..., 0] * eased_t
        map_y = grid_y + flow[..., 1] * eased_t
        intermediate_frame = cv2.remap(first_frame, map_x.astype(np.float32), map_y.astype(np.float32), interpolation=cv2.INTER_LINEAR)

        transition_frames.append(intermediate_frame)

    return transition_frames

def calculate_frame_difference(frame1, frame2):
    """
    Calculate the mean absolute difference between two frames.
    """
    # Ensure both frames are in grayscale
    diff = cv2.absdiff(frame1.astype('float32'), frame2.astype('float32'))
    mean_diff = np.mean(diff)
    return mean_diff

def stabilize_frames_StackReg(frames, threshold=10):
    """
    Stabilize frames only if consecutive frames are not too different.

    Args:
        frames (numpy.ndarray): A 3D numpy array where each slice is a frame.
        threshold (float): Threshold for deciding if frames are too different.

    Returns:
        numpy.ndarray: Stabilized frames, skipping stabilization where frames differ too much.
    """
    sr = StackReg(StackReg.RIGID_BODY)
    stabilized_frames = [frames[0]]  # Initialize with the first frame

    # Use the first frame as the initial reference frame
    ref_frame = frames[0]

    for i in range(1, len(frames)):
        if calculate_frame_difference(ref_frame, frames[i]) < threshold:
            # Frames are similar enough; proceed with stabilization
            transformed_img = sr.register_transform(ref=ref_frame, mov=frames[i])
            stabilized_frames.append(transformed_img)
            ref_frame = transformed_img  
        else:
            # Frames are too different; skip stabilization and use the current frame as is
            stabilized_frames.append(frames[i])
            ref_frame = frames[i] 

    return np.array(stabilized_frames)

def stabilize_frames(frames):
    """
    Stabilize a list of grayscale frames using optical flow and affine transformation with error handling.

    Args:
        frames (list of numpy.ndarray): List of grayscale video frames to stabilize.

    Returns:
        list of numpy.ndarray: Stabilized frames.
    """
    # adjust feature detection and optical flow parameters to fit the target video
    feature_params = dict(maxCorners=100, qualityLevel=0.02, minDistance=3, blockSize=7)
    lk_params = dict(winSize=(15, 15), maxLevel=3, criteria=(cv2.TERM_CRITERIA_EPS | cv2.TERM_CRITERIA_COUNT, 30, 0.01))
    
    stabilized_frames = [frames[0]]
    transformations = [np.eye(2, 3, dtype=np.float32)]  # Identity transformation

    for i in range(1, len(frames)):
        prev_gray = frames[i-1]
        curr_gray = frames[i]

        prev_pts = cv2.goodFeaturesToTrack(prev_gray, **feature_params)
        if prev_pts is not None:
            curr_pts, status, err = cv2.calcOpticalFlowPyrLK(prev_gray, curr_gray, prev_pts, None, **lk_params)
            if curr_pts is not None and status.sum() > 0:
                good_prev_pts = prev_pts[status == 1]
                good_curr_pts = curr_pts[status == 1]
                matrix, inliers = cv2.estimateAffinePartial2D(good_prev_pts, good_curr_pts)
                if matrix is not None:
                    transformations.append(matrix)
                else:
                    transformations.append(transformations[-1])
            else:
                transformations.append(transformations[-1])
        else:
            transformations.append(transformations[-1])

    for i in range(1, len(frames)):
        frame = cv2.warpAffine(frames[i], transformations[i], (frames[i].shape[1], frames[i].shape[0]))
        stabilized_frames.append(frame)

    return stabilized_frames