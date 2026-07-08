# Visual Homing camera profile for the Arducam / OmniVision OV9281 wide-angle mono camera.
# FOV values are nominal placeholders for the 160-degree lens and must be measured for the exact crop.
id = ov9281-160-wide
sensor_type = Visible
pixel_format = Gray8
capture_width = 640
capture_height = 400
target_width = 160
target_height = 100
horizontal_fov_rad = 2.79
vertical_fov_rad = 2.18
low_texture_range_threshold = 4.0
ambiguous_mean_abs_diff_threshold = 2.0
maximum_low_texture_fraction = 0.05
maximum_ambiguous_nearest_fraction = 0.10
minimum_average_nearest_mean_abs_diff = 5.0
