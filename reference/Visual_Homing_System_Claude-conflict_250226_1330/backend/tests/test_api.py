import pytest
import requests
import os

# Get backend URL from environment or use public preview URL
BASE_URL = os.environ.get('REACT_APP_BACKEND_URL', 'http://localhost:8001').rstrip('/')


class TestRootAPI:
    """Test root API endpoint"""
    
    def test_root_endpoint(self, api_client):
        """Test GET /api/ returns API info"""
        response = api_client.get(f"{BASE_URL}/api/")
        assert response.status_code == 200
        data = response.json()
        assert "message" in data
        assert data["message"] == "Visual Homing API"
        assert "version" in data
        assert data["version"] == "1.0"


class TestDocumentation:
    """Test documentation endpoints"""
    
    def test_docs_list_returns_array(self, api_client):
        """Test GET /api/docs/list returns array of documents"""
        response = api_client.get(f"{BASE_URL}/api/docs/list")
        assert response.status_code == 200
        data = response.json()
        assert isinstance(data, list)
        assert len(data) > 0
    
    def test_docs_list_has_required_fields(self, api_client):
        """Test each document has name, path, and title"""
        response = api_client.get(f"{BASE_URL}/api/docs/list")
        assert response.status_code == 200
        data = response.json()
        
        for doc in data:
            assert "name" in doc, f"Document missing 'name' field: {doc}"
            assert "path" in doc, f"Document missing 'path' field: {doc}"
            assert "title" in doc, f"Document missing 'title' field: {doc}"
            assert doc["name"].endswith(".md"), f"Document name should be .md file: {doc['name']}"
    
    def test_get_single_doc(self, api_client):
        """Test GET /api/docs/{filename} returns document content"""
        # First get the list
        list_response = api_client.get(f"{BASE_URL}/api/docs/list")
        docs = list_response.json()
        
        if len(docs) > 0:
            # Get first document
            doc_name = docs[0]["name"]
            response = api_client.get(f"{BASE_URL}/api/docs/{doc_name}")
            assert response.status_code == 200
            data = response.json()
            assert "name" in data
            assert "title" in data
            assert "content" in data
            assert "html" in data
            assert len(data["content"]) > 0
            assert len(data["html"]) > 0


class TestRoutes:
    """Test flight routes endpoints"""
    
    def test_demo_route_generate(self, api_client):
        """Test GET /api/routes/demo/generate returns route data"""
        response = api_client.get(f"{BASE_URL}/api/routes/demo/generate")
        assert response.status_code == 200
        data = response.json()
        
        # Check required fields
        assert "id" in data
        assert "name" in data
        assert "points" in data
        assert "keyframes" in data
        assert "total_distance" in data
        assert "created_at" in data
    
    def test_demo_route_has_points(self, api_client):
        """Test demo route has correct number of points"""
        response = api_client.get(f"{BASE_URL}/api/routes/demo/generate")
        data = response.json()
        
        assert len(data["points"]) == 100, "Should have 100 route points"
        
        # Verify point structure
        first_point = data["points"][0]
        assert "x" in first_point
        assert "y" in first_point
        assert "z" in first_point
        assert "yaw" in first_point
        assert "timestamp" in first_point
        assert "is_keyframe" in first_point
    
    def test_demo_route_has_keyframes(self, api_client):
        """Test demo route has correct number of keyframes"""
        response = api_client.get(f"{BASE_URL}/api/routes/demo/generate")
        data = response.json()
        
        # Should have 10 keyframes (every 10th point)
        assert len(data["keyframes"]) == 10, "Should have 10 keyframes"
        
        # All keyframes should have is_keyframe=True
        for kf in data["keyframes"]:
            assert kf["is_keyframe"] == True
    
    def test_demo_route_total_distance(self, api_client):
        """Test demo route has calculated total distance"""
        response = api_client.get(f"{BASE_URL}/api/routes/demo/generate")
        data = response.json()
        
        # Should have a positive distance
        assert data["total_distance"] > 0
        # Reasonable range for spiral route (should be ~300-500m)
        assert data["total_distance"] > 200
        assert data["total_distance"] < 1000
    
    def test_routes_list(self, api_client):
        """Test GET /api/routes returns list"""
        response = api_client.get(f"{BASE_URL}/api/routes")
        assert response.status_code == 200
        data = response.json()
        assert isinstance(data, list)

    def test_create_and_delete_route(self, api_client):
        """Test POST /api/routes creates route and DELETE removes it"""
        test_route = {
            "id": f"test_route_{int(__import__('time').time() * 1000)}",
            "name": "TEST_Pytest_Route",
            "points": [{"x": 0, "y": 0, "z": 5, "yaw": 0, "timestamp": 0, "is_keyframe": True}],
            "keyframes": [{"x": 0, "y": 0, "z": 5, "yaw": 0, "timestamp": 0, "is_keyframe": True}],
            "total_distance": 100.0,
            "created_at": "2026-02-24T00:00:00Z"
        }
        
        # Create route
        create_response = api_client.post(f"{BASE_URL}/api/routes", json=test_route)
        assert create_response.status_code == 200
        create_data = create_response.json()
        assert create_data["success"] == True
        assert create_data["id"] == test_route["id"]
        
        # Verify in list
        list_response = api_client.get(f"{BASE_URL}/api/routes")
        routes = list_response.json()
        found = [r for r in routes if r["id"] == test_route["id"]]
        assert len(found) == 1
        assert found[0]["name"] == test_route["name"]
        
        # Get single route
        get_response = api_client.get(f"{BASE_URL}/api/routes/{test_route['id']}")
        assert get_response.status_code == 200
        get_data = get_response.json()
        assert get_data["id"] == test_route["id"]
        assert get_data["name"] == test_route["name"]
        
        # Delete route
        delete_response = api_client.delete(f"{BASE_URL}/api/routes/{test_route['id']}")
        assert delete_response.status_code == 200
        delete_data = delete_response.json()
        assert delete_data["success"] == True
        
        # Verify deleted
        list_after_delete = api_client.get(f"{BASE_URL}/api/routes")
        routes_after = list_after_delete.json()
        not_found = [r for r in routes_after if r["id"] == test_route["id"]]
        assert len(not_found) == 0

    def test_delete_nonexistent_route(self, api_client):
        """Test DELETE /api/routes/{id} for non-existent route returns error"""
        response = api_client.delete(f"{BASE_URL}/api/routes/nonexistent_route_12345")
        assert response.status_code == 200  # API returns 200 with error message
        data = response.json()
        assert "error" in data



class TestDronePosition:
    """Test drone position endpoints"""
    
    def test_get_position(self, api_client):
        """Test GET /api/position returns drone position"""
        response = api_client.get(f"{BASE_URL}/api/position")
        assert response.status_code == 200
        data = response.json()
        
        # Check required fields
        assert "x" in data
        assert "y" in data
        assert "z" in data
        assert "yaw" in data
        assert "pitch" in data
        assert "roll" in data
        assert "speed" in data
        assert "mode" in data
    
    def test_post_position(self, api_client):
        """Test POST /api/position updates position"""
        new_position = {
            "x": 10.5,
            "y": 20.3,
            "z": 15.0,
            "yaw": 1.5,
            "pitch": 0.1,
            "roll": 0.2,
            "speed": 5.0,
            "mode": "FLYING"
        }
        
        response = api_client.post(f"{BASE_URL}/api/position", json=new_position)
        assert response.status_code == 200
        data = response.json()
        assert data["success"] == True
        
        # Verify position was updated by GET
        get_response = api_client.get(f"{BASE_URL}/api/position")
        updated_data = get_response.json()
        assert updated_data["x"] == new_position["x"]
        assert updated_data["y"] == new_position["y"]
        assert updated_data["z"] == new_position["z"]
        assert updated_data["mode"] == new_position["mode"]


class TestSimulation:
    """Test simulation endpoints"""
    
    def test_start_simulation(self, api_client):
        """Test GET /api/simulation/start/{route_id} returns message"""
        response = api_client.get(f"{BASE_URL}/api/simulation/start/demo_route_001")
        assert response.status_code == 200
        data = response.json()
        assert "message" in data
        assert "route_id" in data
        assert data["route_id"] == "demo_route_001"


class TestFirmware:
    """Test firmware endpoints"""
    
    def test_firmware_structure(self, api_client):
        """Test GET /api/firmware/structure returns file structure"""
        response = api_client.get(f"{BASE_URL}/api/firmware/structure")
        assert response.status_code == 200
        data = response.json()
        
        # Check expected categories
        assert "python" in data
        assert "cpp" in data
        assert "scripts" in data
        assert "config" in data
        
        # Should have some files
        total_files = len(data["python"]) + len(data["cpp"]) + len(data["scripts"]) + len(data["config"])
        assert total_files > 0, "Should have some firmware files"


class TestSensors:
    """Test sensor status endpoints (MATEK 3901-L0X Optical Flow + TF-Luna LiDAR)"""
    
    def test_get_sensor_status(self, api_client):
        """Test GET /api/sensors/status returns sensor data"""
        response = api_client.get(f"{BASE_URL}/api/sensors/status")
        assert response.status_code == 200
        data = response.json()
        
        # Check optical flow fields
        assert "optical_flow_connected" in data
        assert "optical_flow_quality" in data
        assert "flow_x" in data
        assert "flow_y" in data
        
        # Check LiDAR fields
        assert "lidar_connected" in data
        assert "lidar_distance_m" in data
        assert "lidar_signal" in data

    def test_post_sensor_status(self, api_client):
        """Test POST /api/sensors/status updates sensor data"""
        sensor_data = {
            "optical_flow_connected": True,
            "optical_flow_quality": 85,
            "flow_x": 1.5,
            "flow_y": -0.3,
            "lidar_connected": True,
            "lidar_distance_m": 2.5,
            "lidar_signal": 200
        }
        
        response = api_client.post(f"{BASE_URL}/api/sensors/status", json=sensor_data)
        assert response.status_code == 200
        data = response.json()
        assert data["success"] == True
        
        # Verify with GET
        get_response = api_client.get(f"{BASE_URL}/api/sensors/status")
        updated_data = get_response.json()
        assert updated_data["optical_flow_connected"] == True
        assert updated_data["optical_flow_quality"] == 85
        assert updated_data["lidar_connected"] == True
        assert updated_data["lidar_distance_m"] == 2.5


class TestSmartRTL:
    """Test Smart RTL endpoints (hybrid navigation: IMU/Baro >50m, Optical Flow + Visual <50m)"""
    
    def test_get_smart_rtl_status(self, api_client):
        """Test GET /api/smart-rtl/status returns status"""
        response = api_client.get(f"{BASE_URL}/api/smart-rtl/status")
        assert response.status_code == 200
        data = response.json()
        
        # Check required fields
        assert "active" in data
        assert "phase" in data
        assert "current_altitude" in data
        assert "home_distance" in data
        assert "return_progress" in data
        assert "nav_source" in data
        assert "target_altitude" in data

    def test_post_smart_rtl_status(self, api_client):
        """Test POST /api/smart-rtl/status updates status"""
        rtl_data = {
            "active": True,
            "phase": "descending",
            "current_altitude": 35.0,
            "home_distance": 150.0,
            "return_progress": 0.65,
            "nav_source": "optical_flow",
            "target_altitude": 10.0
        }
        
        response = api_client.post(f"{BASE_URL}/api/smart-rtl/status", json=rtl_data)
        assert response.status_code == 200
        data = response.json()
        assert data["success"] == True
        
        # Verify with GET
        get_response = api_client.get(f"{BASE_URL}/api/smart-rtl/status")
        updated_data = get_response.json()
        assert updated_data["active"] == True
        assert updated_data["phase"] == "descending"
        assert updated_data["nav_source"] == "optical_flow"
        assert updated_data["return_progress"] == 0.65

    def test_get_smart_rtl_config(self, api_client):
        """Test GET /api/smart-rtl/config returns configuration defaults"""
        response = api_client.get(f"{BASE_URL}/api/smart-rtl/config")
        assert response.status_code == 200
        data = response.json()
        
        # Check expected config fields
        assert "high_alt_threshold" in data
        assert data["high_alt_threshold"] == 50.0  # 50m threshold for hybrid nav
        
        assert "precision_land_alt" in data
        assert "descent_rate" in data
        assert "high_alt_speed" in data
        assert "low_alt_speed" in data
        assert "flow_min_quality" in data
        assert "visual_min_confidence" in data


class TestDocumentationUpdated:
    """Test updated documentation with 9 documents"""
    
    def test_docs_list_count(self, api_client):
        """Test GET /api/docs/list returns expected documents"""
        response = api_client.get(f"{BASE_URL}/api/docs/list")
        assert response.status_code == 200
        data = response.json()

        assert len(data) >= 10, f"Expected at least 10 documents, got {len(data)}"
        
        # Verify expected documents exist
        doc_names = [d["name"] for d in data]
        expected_docs = [
            "01_raspberry_pi_setup.md",
            "02_wiring_diagrams.md",
            "03_ardupilot_config.md",
            "04_python_implementation.md",
            "08_testing.md",
            "09_sitl_testing.md"
        ]
        for expected in expected_docs:
            assert expected in doc_names, f"Missing document: {expected}"


    def test_get_sitl_doc(self, api_client):
        """Test GET /api/docs/09_sitl_testing.md returns SITL documentation"""
        response = api_client.get(f"{BASE_URL}/api/docs/09_sitl_testing.md")
        assert response.status_code == 200
        data = response.json()
        
        assert "name" in data
        assert data["name"] == "09_sitl_testing.md"
        assert "title" in data
        assert "SITL" in data["title"]
        assert "content" in data
        assert "SITL" in data["content"]
        assert "ArduPilot" in data["content"]


class TestSettings:
    """Test Settings CRUD endpoints (NEW: v2.2 feature)"""
    
    def test_get_settings_returns_defaults(self, api_client):
        """Test GET /api/settings returns settings with expected fields"""
        response = api_client.get(f"{BASE_URL}/api/settings")
        assert response.status_code == 200
        data = response.json()
        
        # Check camera settings
        assert "camera_type" in data
        assert "camera_device" in data
        assert "camera_resolution_w" in data
        assert "camera_resolution_h" in data
        assert "camera_fps" in data
        
        # Check MAVLink settings
        assert "mavlink_port" in data
        assert "mavlink_baud" in data
        
        # Check Optical Flow settings
        assert "flow_enabled" in data
        assert "flow_port" in data
        assert "flow_min_quality" in data
        
        # Check LiDAR settings
        assert "lidar_enabled" in data
        assert "lidar_port" in data
        
        # Check Smart RTL settings
        assert "rtl_high_alt" in data
        assert "rtl_precision_alt" in data
        assert "rtl_descent_pct" in data
        assert "rtl_descent_rate" in data
        assert "rtl_high_speed" in data
        assert "rtl_low_speed" in data
        
        # Check system settings
        assert "web_port" in data
        assert "autostart" in data
        assert "stream_enabled" in data
    
    def test_post_settings_updates_config(self, api_client):
        """Test POST /api/settings updates configuration"""
        # Get current settings
        current = api_client.get(f"{BASE_URL}/api/settings").json()
        
        # Modify some settings
        modified = current.copy()
        modified["camera_fps"] = 60
        modified["flow_min_quality"] = 75
        modified["rtl_high_alt"] = 60.0
        
        # Save settings
        response = api_client.post(f"{BASE_URL}/api/settings", json=modified)
        assert response.status_code == 200
        data = response.json()
        assert data["success"] == True
        
        # Verify settings were saved
        verify_response = api_client.get(f"{BASE_URL}/api/settings")
        verify_data = verify_response.json()
        assert verify_data["camera_fps"] == 60
        assert verify_data["flow_min_quality"] == 75
        assert verify_data["rtl_high_alt"] == 60.0
        
        # Reset settings back to defaults
        api_client.post(f"{BASE_URL}/api/settings/reset")
    
    def test_reset_settings_restores_defaults(self, api_client):
        """Test POST /api/settings/reset resets to defaults"""
        response = api_client.post(f"{BASE_URL}/api/settings/reset")
        assert response.status_code == 200
        data = response.json()
        
        # Check default values are restored
        assert data["camera_type"] == "usb_capture"
        assert data["camera_device"] == "/dev/video0"
        assert data["camera_fps"] == 30
        assert data["mavlink_baud"] == 115200
        assert data["flow_min_quality"] == 50
        assert data["rtl_high_alt"] == 50.0
        assert data["rtl_precision_alt"] == 5.0


class TestStreamStatus:
    """Test Video Stream status endpoint (NEW: v2.2 feature)"""
    
    def test_stream_status_returns_info(self, api_client):
        """Test GET /api/stream/status returns stream information"""
        response = api_client.get(f"{BASE_URL}/api/stream/status")
        assert response.status_code == 200
        data = response.json()
        
        # Check required fields
        assert "available" in data
        assert "message" in data
        assert "url" in data
        assert "type" in data
        
        # In preview environment, stream is not available
        assert data["available"] == False
        assert data["url"] == "/api/stream/video"
        assert data["type"] == "mjpeg"


class TestRouteExport:
    """Test Route Export endpoints (NEW: v2.2 feature)"""
    
    def test_export_route_json(self, api_client):
        """Test GET /api/routes/{id}/export/json returns JSON data"""
        # First create a test route
        test_route = {
            "id": f"test_export_json_{int(__import__('time').time() * 1000)}",
            "name": "TEST_Export_JSON",
            "points": [
                {"x": 0, "y": 0, "z": 5, "yaw": 0, "timestamp": 0, "is_keyframe": True},
                {"x": 10, "y": 10, "z": 10, "yaw": 0.5, "timestamp": 1, "is_keyframe": False}
            ],
            "keyframes": [{"x": 0, "y": 0, "z": 5, "yaw": 0, "timestamp": 0, "is_keyframe": True}],
            "total_distance": 14.14,
            "created_at": "2026-02-28T00:00:00Z"
        }
        
        # Create the route
        create_response = api_client.post(f"{BASE_URL}/api/routes", json=test_route)
        assert create_response.status_code == 200
        
        try:
            # Export as JSON
            export_response = api_client.get(f"{BASE_URL}/api/routes/{test_route['id']}/export/json")
            assert export_response.status_code == 200
            data = export_response.json()
            
            # Verify exported data matches
            assert data["id"] == test_route["id"]
            assert data["name"] == test_route["name"]
            assert len(data["points"]) == 2
            assert len(data["keyframes"]) == 1
            assert data["total_distance"] == 14.14
        finally:
            # Cleanup
            api_client.delete(f"{BASE_URL}/api/routes/{test_route['id']}")
    
    def test_export_route_kml(self, api_client):
        """Test GET /api/routes/{id}/export/kml returns valid KML"""
        # First create a test route
        test_route = {
            "id": f"test_export_kml_{int(__import__('time').time() * 1000)}",
            "name": "TEST_Export_KML",
            "points": [
                {"x": 0, "y": 0, "z": 5, "yaw": 0, "timestamp": 0, "is_keyframe": True},
                {"x": 10, "y": 10, "z": 10, "yaw": 0.5, "timestamp": 1, "is_keyframe": False}
            ],
            "keyframes": [{"x": 0, "y": 0, "z": 5, "yaw": 0, "timestamp": 0, "is_keyframe": True}],
            "total_distance": 14.14,
            "created_at": "2026-02-28T00:00:00Z"
        }
        
        # Create the route
        create_response = api_client.post(f"{BASE_URL}/api/routes", json=test_route)
        assert create_response.status_code == 200
        
        try:
            # Export as KML
            export_response = api_client.get(f"{BASE_URL}/api/routes/{test_route['id']}/export/kml")
            assert export_response.status_code == 200
            
            # Check content type is KML
            content_type = export_response.headers.get("content-type", "")
            assert "kml" in content_type.lower() or "xml" in content_type.lower()
            
            # Verify KML structure
            kml_content = export_response.text
            assert '<?xml version="1.0"' in kml_content
            assert '<kml xmlns="http://www.opengis.net/kml/2.2">' in kml_content
            assert f'<name>{test_route["name"]}</name>' in kml_content
            assert '<coordinates>' in kml_content
            assert '</coordinates>' in kml_content
            assert '<LineString>' in kml_content
        finally:
            # Cleanup
            api_client.delete(f"{BASE_URL}/api/routes/{test_route['id']}")
    
    def test_export_nonexistent_route_returns_error(self, api_client):
        """Test export of non-existent route returns error"""
        json_response = api_client.get(f"{BASE_URL}/api/routes/nonexistent_12345/export/json")
        assert json_response.status_code == 200
        json_data = json_response.json()
        assert "error" in json_data
        
        kml_response = api_client.get(f"{BASE_URL}/api/routes/nonexistent_12345/export/kml")
        assert kml_response.status_code == 200
        kml_data = kml_response.json()
        assert "error" in kml_data
