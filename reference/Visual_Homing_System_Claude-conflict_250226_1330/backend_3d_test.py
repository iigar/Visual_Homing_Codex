#!/usr/bin/env python3
"""
Backend API Testing for Visual Homing 3D Map System
Tests the new 3D map visualization APIs
"""
import requests
import sys
import json
from datetime import datetime

class Map3DAPITester:
    def __init__(self, base_url="http://localhost:8001"):
        self.base_url = base_url
        self.api_url = f"{base_url}/api"
        self.tests_run = 0
        self.tests_passed = 0
        self.failed_tests = []

    def log_test(self, name, success, details=""):
        """Log test result"""
        self.tests_run += 1
        if success:
            self.tests_passed += 1
            print(f"✅ {name}")
        else:
            print(f"❌ {name} - {details}")
            self.failed_tests.append({"test": name, "error": details})

    def test_demo_route_generation(self):
        """Test GET /api/routes/demo/generate - generates demo route with points and keyframes"""
        try:
            response = requests.get(f"{self.api_url}/routes/demo/generate", timeout=15)
            if response.status_code == 200:
                data = response.json()
                required_fields = ["id", "name", "points", "keyframes", "total_distance", "created_at"]
                
                # Check all required fields exist
                missing_fields = [field for field in required_fields if field not in data]
                if missing_fields:
                    self.log_test("Demo route generation", False, f"Missing fields: {missing_fields}")
                    return False
                
                # Validate points structure
                if not isinstance(data["points"], list) or len(data["points"]) == 0:
                    self.log_test("Demo route generation", False, "Points field is not a valid list or is empty")
                    return False
                
                # Check first point structure
                first_point = data["points"][0]
                point_fields = ["x", "y", "z", "yaw", "timestamp", "is_keyframe"]
                missing_point_fields = [field for field in point_fields if field not in first_point]
                if missing_point_fields:
                    self.log_test("Demo route generation", False, f"Point missing fields: {missing_point_fields}")
                    return False
                
                # Validate keyframes
                if not isinstance(data["keyframes"], list):
                    self.log_test("Demo route generation", False, "Keyframes field is not a list")
                    return False
                
                # Validate numeric values
                if not isinstance(data["total_distance"], (int, float)):
                    self.log_test("Demo route generation", False, "total_distance is not numeric")
                    return False
                
                self.log_test("Demo route generation", True)
                print(f"   Generated route: {data['name']} with {len(data['points'])} points and {len(data['keyframes'])} keyframes")
                print(f"   Total distance: {data['total_distance']:.1f}m")
                return True
            else:
                self.log_test("Demo route generation", False, f"Status: {response.status_code}")
        except Exception as e:
            self.log_test("Demo route generation", False, f"Error: {str(e)}")
        return False

    def test_drone_position_get(self):
        """Test GET /api/position - returns current drone position"""
        try:
            response = requests.get(f"{self.api_url}/position", timeout=10)
            if response.status_code == 200:
                data = response.json()
                required_fields = ["x", "y", "z", "yaw", "pitch", "roll", "speed", "mode"]
                
                missing_fields = [field for field in required_fields if field not in data]
                if missing_fields:
                    self.log_test("Drone position GET", False, f"Missing fields: {missing_fields}")
                    return False
                
                # Validate numeric fields
                numeric_fields = ["x", "y", "z", "yaw", "pitch", "roll", "speed"]
                for field in numeric_fields:
                    if not isinstance(data[field], (int, float)):
                        self.log_test("Drone position GET", False, f"{field} is not numeric")
                        return False
                
                # Validate mode field
                if not isinstance(data["mode"], str):
                    self.log_test("Drone position GET", False, "mode is not a string")
                    return False
                
                self.log_test("Drone position GET", True)
                print(f"   Position: ({data['x']:.1f}, {data['y']:.1f}, {data['z']:.1f}), Mode: {data['mode']}")
                return True
            else:
                self.log_test("Drone position GET", False, f"Status: {response.status_code}")
        except Exception as e:
            self.log_test("Drone position GET", False, f"Error: {str(e)}")
        return False

    def test_drone_position_post(self):
        """Test POST /api/position - updates drone position"""
        try:
            test_position = {
                "x": 10.5,
                "y": 5.2,
                "z": 8.0,
                "yaw": 1.57,
                "pitch": 0.1,
                "roll": -0.05,
                "speed": 2.5,
                "mode": "FLYING"
            }
            
            response = requests.post(f"{self.api_url}/position", json=test_position, timeout=10)
            if response.status_code == 200:
                data = response.json()
                if "success" in data and data["success"]:
                    # Verify the position was updated by getting it back
                    get_response = requests.get(f"{self.api_url}/position", timeout=10)
                    if get_response.status_code == 200:
                        updated_pos = get_response.json()
                        if (updated_pos["x"] == test_position["x"] and 
                            updated_pos["y"] == test_position["y"] and
                            updated_pos["mode"] == test_position["mode"]):
                            self.log_test("Drone position POST", True)
                            return True
                        else:
                            self.log_test("Drone position POST", False, "Position not updated correctly")
                    else:
                        self.log_test("Drone position POST", False, "Failed to verify position update")
                else:
                    self.log_test("Drone position POST", False, "Response missing success field")
            else:
                self.log_test("Drone position POST", False, f"Status: {response.status_code}")
        except Exception as e:
            self.log_test("Drone position POST", False, f"Error: {str(e)}")
        return False

    def test_simulation_start(self):
        """Test GET /api/simulation/start/{route_id} - starts route simulation"""
        try:
            test_route_id = "demo_route_001"
            response = requests.get(f"{self.api_url}/simulation/start/{test_route_id}", timeout=10)
            if response.status_code == 200:
                data = response.json()
                if "message" in data and "route_id" in data:
                    if data["route_id"] == test_route_id:
                        self.log_test("Simulation start", True)
                        return True
                    else:
                        self.log_test("Simulation start", False, "Route ID mismatch in response")
                else:
                    self.log_test("Simulation start", False, "Missing message or route_id fields")
            else:
                self.log_test("Simulation start", False, f"Status: {response.status_code}")
        except Exception as e:
            self.log_test("Simulation start", False, f"Error: {str(e)}")
        return False

    def test_routes_list(self):
        """Test GET /api/routes - lists saved routes"""
        try:
            response = requests.get(f"{self.api_url}/routes", timeout=10)
            if response.status_code == 200:
                data = response.json()
                if isinstance(data, list):
                    self.log_test("Routes list", True)
                    print(f"   Found {len(data)} saved routes")
                    return True
                else:
                    self.log_test("Routes list", False, "Response is not a list")
            else:
                self.log_test("Routes list", False, f"Status: {response.status_code}")
        except Exception as e:
            self.log_test("Routes list", False, f"Error: {str(e)}")
        return False

    def run_all_tests(self):
        """Run all 3D map backend API tests"""
        print("🔍 Starting Visual Homing 3D Map Backend API Tests")
        print(f"📡 Base URL: {self.base_url}")
        print("-" * 60)

        # Test the key 3D map APIs
        self.test_demo_route_generation()
        self.test_drone_position_get()
        self.test_drone_position_post()
        self.test_simulation_start()
        self.test_routes_list()

        # Final results
        print("-" * 60)
        print(f"📊 Tests completed: {self.tests_passed}/{self.tests_run} passed")
        
        if self.failed_tests:
            print("\n❌ Failed tests:")
            for failure in self.failed_tests:
                print(f"   • {failure['test']}: {failure['error']}")
        else:
            print("🎉 All 3D Map API tests passed!")
        
        return self.tests_passed == self.tests_run

def main():
    """Main test execution"""
    tester = Map3DAPITester()
    success = tester.run_all_tests()
    
    # Return appropriate exit code
    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())