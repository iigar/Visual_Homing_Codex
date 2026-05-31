# Visual Homing camera profile for the Raspberry Pi Camera Module v2 / IMX219 baseline.
# FOV values are placeholders until measured for the exact lens/crop mode.
id = imx219-visible-wide
sensor_type = Visible
pixel_format = Gray8
capture_width = 320
capture_height = 240
target_width = 32
target_height = 24
horizontal_fov_rad = 1.08
vertical_fov_rad = 0.83
low_texture_range_threshold = 4.0
ambiguous_mean_abs_diff_threshold = 2.0
maximum_low_texture_fraction = 0.05
maximum_ambiguous_nearest_fraction = 0.10
minimum_average_nearest_mean_abs_diff = 5.0
