#!/usr/bin/env python3
"""
Backend API Testing for Visual Homing Documentation System
Tests all API endpoints defined in the review request
"""
import requests
import sys
import json
from datetime import datetime

class VisualHomingAPITester:
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

    def test_root_endpoint(self):
        """Test GET /api/ endpoint"""
        try:
            response = requests.get(f"{self.api_url}/", timeout=10)
            if response.status_code == 200:
                data = response.json()
                if "message" in data and "version" in data:
                    self.log_test("Root API endpoint", True)
                    return True
                else:
                    self.log_test("Root API endpoint", False, "Missing message or version fields")
            else:
                self.log_test("Root API endpoint", False, f"Status: {response.status_code}")
        except Exception as e:
            self.log_test("Root API endpoint", False, f"Error: {str(e)}")
        return False

    def test_docs_list(self):
        """Test GET /api/docs/list endpoint"""
        try:
            response = requests.get(f"{self.api_url}/docs/list", timeout=10)
            if response.status_code == 200:
                data = response.json()
                if isinstance(data, list):
                    self.log_test("Docs list endpoint", True)
                    print(f"   Found {len(data)} documentation files")
                    return data
                else:
                    self.log_test("Docs list endpoint", False, "Response is not a list")
            else:
                self.log_test("Docs list endpoint", False, f"Status: {response.status_code}")
        except Exception as e:
            self.log_test("Docs list endpoint", False, f"Error: {str(e)}")
        return []

    def test_docs_content(self, filename):
        """Test GET /api/docs/{filename} endpoint"""
        try:
            response = requests.get(f"{self.api_url}/docs/{filename}", timeout=10)
            if response.status_code == 200:
                data = response.json()
                required_fields = ["name", "title", "content", "html"]
                if all(field in data for field in required_fields):
                    self.log_test(f"Doc content: {filename}", True)
                    return True
                else:
                    missing = [f for f in required_fields if f not in data]
                    self.log_test(f"Doc content: {filename}", False, f"Missing fields: {missing}")
            else:
                self.log_test(f"Doc content: {filename}", False, f"Status: {response.status_code}")
        except Exception as e:
            self.log_test(f"Doc content: {filename}", False, f"Error: {str(e)}")
        return False

    def test_firmware_structure(self):
        """Test GET /api/firmware/structure endpoint"""
        try:
            response = requests.get(f"{self.api_url}/firmware/structure", timeout=10)
            if response.status_code == 200:
                data = response.json()
                expected_keys = ["python", "cpp", "scripts", "config"]
                if all(key in data for key in expected_keys):
                    self.log_test("Firmware structure endpoint", True)
                    for key, files in data.items():
                        print(f"   {key}: {len(files)} files")
                    return data
                else:
                    missing = [k for k in expected_keys if k not in data]
                    self.log_test("Firmware structure endpoint", False, f"Missing keys: {missing}")
            else:
                self.log_test("Firmware structure endpoint", False, f"Status: {response.status_code}")
        except Exception as e:
            self.log_test("Firmware structure endpoint", False, f"Error: {str(e)}")
        return {}

    def test_firmware_file(self, filepath):
        """Test GET /api/firmware/file/{filepath} endpoint"""
        try:
            # URL encode the filepath properly
            url = f"{self.api_url}/firmware/file/{filepath}"
            response = requests.get(url, timeout=10)
            if response.status_code == 200:
                data = response.json()
                if "path" in data and "content" in data:
                    self.log_test(f"Firmware file: {filepath}", True)
                    return True
                else:
                    self.log_test(f"Firmware file: {filepath}", False, "Missing path or content fields")
            else:
                self.log_test(f"Firmware file: {filepath}", False, f"Status: {response.status_code}")
        except Exception as e:
            self.log_test(f"Firmware file: {filepath}", False, f"Error: {str(e)}")
        return False

    def run_all_tests(self):
        """Run all backend API tests"""
        print("🔍 Starting Visual Homing Backend API Tests")
        print(f"📡 Base URL: {self.base_url}")
        print("-" * 60)

        # Test root endpoint
        self.test_root_endpoint()

        # Test documentation endpoints
        docs = self.test_docs_list()
        if docs:
            # Test first few doc files
            test_docs = docs[:3] if len(docs) > 3 else docs
            for doc in test_docs:
                if isinstance(doc, dict) and "name" in doc:
                    self.test_docs_content(doc["name"])

        # Test firmware endpoints
        firmware_structure = self.test_firmware_structure()
        if firmware_structure:
            # Test a few firmware files from each category
            test_files = []
            for category, files in firmware_structure.items():
                if files:
                    test_files.extend(files[:2])  # Test first 2 from each category
            
            for filepath in test_files[:5]:  # Limit to 5 files total
                self.test_firmware_file(filepath)

        # Final results
        print("-" * 60)
        print(f"📊 Tests completed: {self.tests_passed}/{self.tests_run} passed")
        
        if self.failed_tests:
            print("\n❌ Failed tests:")
            for failure in self.failed_tests:
                print(f"   • {failure['test']}: {failure['error']}")
        
        return self.tests_passed == self.tests_run

def main():
    """Main test execution"""
    tester = VisualHomingAPITester()
    success = tester.run_all_tests()
    
    # Return appropriate exit code
    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())