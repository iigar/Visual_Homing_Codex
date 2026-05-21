"""
Unit tests for SmartRTL — no SITL / hardware required.
Tests phase transitions, nav source selection, velocity commands, and edge cases.
"""
import sys
import os
import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..'))

from navigation.smart_rtl import SmartRTL, SmartRTLConfig, SmartRTLPhase


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def make_rtl(**kwargs) -> SmartRTL:
    return SmartRTL(SmartRTLConfig(**kwargs))


# ---------------------------------------------------------------------------
# initiate_rtl — initial phase selection
# ---------------------------------------------------------------------------

class TestInitiateRTL:
    def test_high_altitude_enters_high_alt(self):
        rtl = SmartRTL()
        phase = rtl.initiate_rtl(current_altitude=100, home_distance=5000)
        assert phase == SmartRTLPhase.HIGH_ALT

    def test_low_altitude_enters_low_alt(self):
        rtl = SmartRTL()
        phase = rtl.initiate_rtl(current_altitude=30, home_distance=500)
        assert phase == SmartRTLPhase.LOW_ALT

    def test_exactly_on_threshold_enters_low_alt(self):
        rtl = make_rtl(high_alt_threshold=50.0)
        phase = rtl.initiate_rtl(current_altitude=50.0, home_distance=100)
        assert phase == SmartRTLPhase.LOW_ALT

    def test_just_above_threshold_enters_high_alt(self):
        rtl = make_rtl(high_alt_threshold=50.0)
        phase = rtl.initiate_rtl(current_altitude=50.1, home_distance=100)
        assert phase == SmartRTLPhase.HIGH_ALT

    def test_nav_source_set_correctly_high(self):
        rtl = SmartRTL()
        rtl.initiate_rtl(current_altitude=200, home_distance=2000)
        assert rtl.state.nav_source == "imu_baro"

    def test_nav_source_set_correctly_low(self):
        rtl = SmartRTL()
        rtl.initiate_rtl(current_altitude=10, home_distance=100)
        assert rtl.state.nav_source == "optical_flow_visual"

    def test_total_return_distance_stored(self):
        rtl = SmartRTL()
        rtl.initiate_rtl(current_altitude=100, home_distance=3000)
        assert rtl.state.total_return_distance == 3000

    def test_is_active_after_initiate(self):
        rtl = SmartRTL()
        rtl.initiate_rtl(current_altitude=100, home_distance=1000)
        assert rtl.is_active


# ---------------------------------------------------------------------------
# Phase transitions via update()
# ---------------------------------------------------------------------------

class TestPhaseTransitions:
    def test_high_alt_to_descent_at_50_pct(self):
        rtl = SmartRTL()
        rtl.initiate_rtl(current_altitude=200, home_distance=5000)
        # 50% covered → 2500 remaining
        rtl.update(altitude=200, home_distance=2500)
        assert rtl.phase == SmartRTLPhase.DESCENT

    def test_high_alt_stays_before_50_pct(self):
        rtl = SmartRTL()
        rtl.initiate_rtl(current_altitude=200, home_distance=5000)
        # 49% covered → 2550 remaining
        rtl.update(altitude=200, home_distance=2550)
        assert rtl.phase == SmartRTLPhase.HIGH_ALT

    def test_descent_to_low_alt_when_altitude_drops(self):
        rtl = make_rtl(high_alt_threshold=50.0)
        rtl.initiate_rtl(current_altitude=200, home_distance=5000)
        rtl.update(altitude=200, home_distance=2500)   # enter DESCENT
        assert rtl.phase == SmartRTLPhase.DESCENT
        rtl.update(altitude=49.9, home_distance=2000)   # drop below threshold
        assert rtl.phase == SmartRTLPhase.LOW_ALT

    def test_low_alt_to_precision_land(self):
        rtl = make_rtl(high_alt_threshold=50.0, precision_land_threshold=5.0)
        rtl.initiate_rtl(current_altitude=3.0, home_distance=5.0)
        assert rtl.phase == SmartRTLPhase.LOW_ALT
        rtl.update(altitude=3.0, home_distance=5.0, flow_quality=80)
        assert rtl.phase == SmartRTLPhase.PRECISION_LAND

    def test_low_alt_no_precision_land_if_far_from_home(self):
        rtl = SmartRTL()
        rtl.initiate_rtl(current_altitude=3.0, home_distance=50.0)
        rtl.update(altitude=3.0, home_distance=50.0, flow_quality=80)
        assert rtl.phase == SmartRTLPhase.LOW_ALT

    def test_precision_land_to_completed(self):
        rtl = make_rtl(precision_land_threshold=5.0, min_altitude=0.3)
        rtl.initiate_rtl(current_altitude=3.0, home_distance=5.0)
        rtl.update(altitude=3.0, home_distance=5.0, flow_quality=80)  # → PRECISION_LAND
        rtl.update(altitude=0.2, home_distance=1.0)                    # → COMPLETED
        assert rtl.phase == SmartRTLPhase.COMPLETED

    def test_custom_descent_start_pct(self):
        rtl = make_rtl(descent_start_pct=0.7)
        rtl.initiate_rtl(current_altitude=200, home_distance=1000)
        rtl.update(altitude=200, home_distance=350)   # 65% covered — not yet
        assert rtl.phase == SmartRTLPhase.HIGH_ALT
        rtl.update(altitude=200, home_distance=290)   # 71% covered
        assert rtl.phase == SmartRTLPhase.DESCENT


# ---------------------------------------------------------------------------
# Return progress
# ---------------------------------------------------------------------------

class TestReturnProgress:
    def test_progress_zero_at_start(self):
        rtl = SmartRTL()
        rtl.initiate_rtl(current_altitude=100, home_distance=1000)
        assert rtl.state.return_progress == 0.0

    def test_progress_increases(self):
        rtl = SmartRTL()
        rtl.initiate_rtl(current_altitude=100, home_distance=1000)
        rtl.update(altitude=100, home_distance=600)
        assert abs(rtl.state.return_progress - 0.4) < 0.01

    def test_progress_capped_at_one(self):
        rtl = SmartRTL()
        rtl.initiate_rtl(current_altitude=100, home_distance=100)
        rtl.update(altitude=100, home_distance=0)
        assert rtl.state.return_progress == 1.0

    def test_progress_not_negative(self):
        rtl = SmartRTL()
        rtl.initiate_rtl(current_altitude=100, home_distance=100)
        rtl.update(altitude=100, home_distance=150)   # overshot
        assert rtl.state.return_progress >= 0.0


# ---------------------------------------------------------------------------
# Navigation source
# ---------------------------------------------------------------------------

class TestNavSource:
    def test_low_alt_flow_only(self):
        rtl = make_rtl(flow_min_quality=50, visual_min_confidence=0.3)
        rtl.initiate_rtl(current_altitude=10, home_distance=100)
        rtl.update(altitude=10, home_distance=80, flow_quality=80, visual_confidence=0.0)
        assert rtl.state.nav_source == "optical_flow"

    def test_low_alt_flow_and_visual(self):
        rtl = make_rtl(flow_min_quality=50, visual_min_confidence=0.3)
        rtl.initiate_rtl(current_altitude=10, home_distance=100)
        rtl.update(altitude=10, home_distance=80, flow_quality=80, visual_confidence=0.5)
        assert rtl.state.nav_source == "optical_flow_visual"

    def test_low_alt_visual_only(self):
        rtl = make_rtl(flow_min_quality=50, visual_min_confidence=0.3)
        rtl.initiate_rtl(current_altitude=10, home_distance=100)
        rtl.update(altitude=10, home_distance=80, flow_quality=10, visual_confidence=0.8)
        assert rtl.state.nav_source == "visual_only"

    def test_low_alt_imu_fallback(self):
        rtl = make_rtl(flow_min_quality=50, visual_min_confidence=0.3)
        rtl.initiate_rtl(current_altitude=10, home_distance=100)
        rtl.update(altitude=10, home_distance=80, flow_quality=0, visual_confidence=0.0)
        assert rtl.state.nav_source == "imu_baro_fallback"

    def test_precision_land_nav_source(self):
        rtl = SmartRTL()
        rtl.initiate_rtl(current_altitude=3.0, home_distance=5.0)
        rtl.update(altitude=3.0, home_distance=5.0, flow_quality=80)
        assert rtl.state.nav_source == "optical_flow_lidar"


# ---------------------------------------------------------------------------
# get_velocity_command
# ---------------------------------------------------------------------------

class TestVelocityCommand:
    def test_inactive_returns_zeros(self):
        rtl = SmartRTL()
        cmd = rtl.get_velocity_command()
        assert cmd["vx"] == 0
        assert cmd["vy"] == 0
        assert cmd["vz"] == 0

    def test_high_alt_autopilot_control(self):
        rtl = SmartRTL()
        rtl.initiate_rtl(current_altitude=100, home_distance=5000)
        cmd = rtl.get_velocity_command()
        assert cmd.get("autopilot_control") is True

    def test_descent_has_positive_vz(self):
        rtl = SmartRTL()
        rtl.initiate_rtl(current_altitude=200, home_distance=5000)
        rtl.update(altitude=200, home_distance=2400)   # → DESCENT
        cmd = rtl.get_velocity_command()
        assert cmd["vz"] > 0

    def test_descent_vz_within_max(self):
        rtl = make_rtl(descent_rate=2.0, max_descent_rate=3.0)
        rtl.initiate_rtl(current_altitude=200, home_distance=5000)
        rtl.update(altitude=200, home_distance=2400)
        cmd = rtl.get_velocity_command()
        assert cmd["vz"] <= 3.0

    def test_low_alt_no_autopilot_control(self):
        rtl = SmartRTL()
        rtl.initiate_rtl(current_altitude=10, home_distance=100)
        cmd = rtl.get_velocity_command()
        assert cmd.get("autopilot_control") is False

    def test_precision_land_slow_descent(self):
        rtl = SmartRTL()
        rtl.initiate_rtl(current_altitude=3.0, home_distance=5.0)
        rtl.update(altitude=3.0, home_distance=5.0, flow_quality=80)
        cmd = rtl.get_velocity_command()
        # descent rate should be ≤ precision_speed
        assert cmd["vz"] <= rtl.config.precision_speed + 0.1


# ---------------------------------------------------------------------------
# abort()
# ---------------------------------------------------------------------------

class TestAbort:
    def test_abort_sets_error_phase(self):
        rtl = SmartRTL()
        rtl.initiate_rtl(current_altitude=100, home_distance=1000)
        rtl.abort()
        assert rtl.phase == SmartRTLPhase.ERROR

    def test_abort_deactivates(self):
        rtl = SmartRTL()
        rtl.initiate_rtl(current_altitude=100, home_distance=1000)
        rtl.abort()
        assert not rtl.is_active

    def test_update_after_abort_ignored(self):
        rtl = SmartRTL()
        rtl.initiate_rtl(current_altitude=100, home_distance=1000)
        rtl.abort()
        state = rtl.update(altitude=0.1, home_distance=0.1)
        assert state.phase == SmartRTLPhase.ERROR
