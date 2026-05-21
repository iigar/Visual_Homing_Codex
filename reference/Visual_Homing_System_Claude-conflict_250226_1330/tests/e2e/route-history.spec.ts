import { test, expect } from '@playwright/test';
import { waitForAppReady, navigateToTab, waitFor3DCanvas } from '../fixtures/helpers';

test.describe('Visual Homing - Route History & Save', () => {
  const baseUrl = process.env.BASE_URL || 'http://localhost:8001';
  let createdRouteId: string | null = null;

  test.afterEach(async ({ request }) => {
    // Cleanup any routes we created
    if (createdRouteId) {
      try {
        await request.delete(`${baseUrl}/api/routes/${createdRouteId}`);
      } catch (e) {
        // Ignore cleanup errors
      }
      createdRouteId = null;
    }
  });

  test('API: List routes endpoint returns array', async ({ request }) => {
    const response = await request.get(`${baseUrl}/api/routes`);
    expect(response.status()).toBe(200);
    const data = await response.json();
    expect(Array.isArray(data)).toBe(true);
  });

  test('API: Save and delete route via API', async ({ request }) => {
    const testRoute = {
      id: `test_route_${Date.now()}`,
      name: `TEST_Route_${Date.now()}`,
      points: [{ x: 0, y: 0, z: 5, yaw: 0, timestamp: 0, is_keyframe: true }],
      keyframes: [{ x: 0, y: 0, z: 5, yaw: 0, timestamp: 0, is_keyframe: true }],
      total_distance: 100,
      created_at: new Date().toISOString()
    };

    // Save route
    const saveResponse = await request.post(`${baseUrl}/api/routes`, { data: testRoute });
    expect(saveResponse.status()).toBe(200);
    const saveResult = await saveResponse.json();
    expect(saveResult.success).toBe(true);
    createdRouteId = testRoute.id;

    // Verify in list
    const listResponse = await request.get(`${baseUrl}/api/routes`);
    const routes = await listResponse.json();
    const found = routes.find((r: any) => r.id === testRoute.id);
    expect(found).toBeDefined();
    expect(found.name).toBe(testRoute.name);

    // Delete route
    const deleteResponse = await request.delete(`${baseUrl}/api/routes/${testRoute.id}`);
    expect(deleteResponse.status()).toBe(200);
    const deleteResult = await deleteResponse.json();
    expect(deleteResult.success).toBe(true);
    createdRouteId = null;

    // Verify deleted
    const afterDeleteResponse = await request.get(`${baseUrl}/api/routes`);
    const afterDeleteRoutes = await afterDeleteResponse.json();
    const notFound = afterDeleteRoutes.find((r: any) => r.id === testRoute.id);
    expect(notFound).toBeUndefined();
  });

  test('UI: Enable auto-save and save route', async ({ page, request }) => {
    await page.goto('/', { waitUntil: 'domcontentloaded' });
    await waitForAppReady(page);
    await waitFor3DCanvas(page);
    
    const mapPanel = page.getByTestId('map-panel');
    
    // Enable auto-save toggle
    const switchToggle = mapPanel.locator('button[role="switch"]');
    await switchToggle.click({ force: true });
    
    // Wait for save button to appear
    const saveBtn = page.getByTestId('save-route-btn');
    await expect(saveBtn).toBeVisible({ timeout: 5000 });
    
    // Get current routes count
    const routesBefore = await request.get(`${baseUrl}/api/routes`);
    const beforeList = await routesBefore.json();
    const countBefore = beforeList.length;
    
    // Click save button
    await saveBtn.click({ force: true });
    
    // Wait for toast/response
    await page.waitForResponse(
      resp => resp.url().includes('/api/routes') && resp.request().method() === 'POST',
      { timeout: 10000 }
    );
    
    // Verify route was saved
    const routesAfter = await request.get(`${baseUrl}/api/routes`);
    const afterList = await routesAfter.json();
    expect(afterList.length).toBe(countBefore + 1);
    
    // Get the new route ID for cleanup
    const newRoute = afterList.find((r: any) => !beforeList.find((b: any) => b.id === r.id));
    if (newRoute) {
      createdRouteId = newRoute.id;
    }
  });

  test('UI: Route History page displays saved routes', async ({ page, request }) => {
    // Create a test route first
    const testRoute = {
      id: `test_route_${Date.now()}`,
      name: `TEST_Display_${Date.now()}`,
      points: [{ x: 0, y: 0, z: 5, yaw: 0, timestamp: 0, is_keyframe: true }],
      keyframes: [{ x: 0, y: 0, z: 5, yaw: 0, timestamp: 0, is_keyframe: true }],
      total_distance: 50.5,
      created_at: new Date().toISOString()
    };
    await request.post(`${baseUrl}/api/routes`, { data: testRoute });
    createdRouteId = testRoute.id;

    // Navigate to history page
    await page.goto('/', { waitUntil: 'domcontentloaded' });
    await waitForAppReady(page);
    await navigateToTab(page, 'tab-history');
    
    const historySection = page.getByTestId('history-section');
    await expect(historySection).toBeVisible();
    
    // Check that route-history list is visible (not empty state)
    const routeHistory = page.getByTestId('route-history');
    await expect(routeHistory).toBeVisible({ timeout: 10000 });
    
    // Find the route we created
    await expect(historySection).toContainText(testRoute.name, { timeout: 5000 });
  });

  test('UI: Delete route from history', async ({ page, request }) => {
    // Create a test route first
    const testRoute = {
      id: `test_route_${Date.now()}`,
      name: `TEST_Delete_${Date.now()}`,
      points: [{ x: 0, y: 0, z: 5, yaw: 0, timestamp: 0, is_keyframe: true }],
      keyframes: [{ x: 0, y: 0, z: 5, yaw: 0, timestamp: 0, is_keyframe: true }],
      total_distance: 75.0,
      created_at: new Date().toISOString()
    };
    await request.post(`${baseUrl}/api/routes`, { data: testRoute });
    createdRouteId = testRoute.id;

    // Navigate to history page
    await page.goto('/', { waitUntil: 'domcontentloaded' });
    await waitForAppReady(page);
    await navigateToTab(page, 'tab-history');
    
    // Wait for routes to load
    const routeHistory = page.getByTestId('route-history');
    await expect(routeHistory).toBeVisible({ timeout: 10000 });
    
    // Find the route card containing our route name
    const routeCard = page.locator(`h3:has-text("${testRoute.name}")`).first();
    await expect(routeCard).toBeVisible();
    
    // The delete button is in the same card wrapper with red/trash icon
    // It's the second button in the action buttons group (first is view/map icon)
    const cardContainer = routeCard.locator('..').locator('..');
    
    // Set up delete response listener before clicking
    const deletePromise = page.waitForResponse(
      resp => resp.url().includes(`/api/routes/${testRoute.id}`) && resp.request().method() === 'DELETE',
      { timeout: 10000 }
    );
    
    // Click the delete button - it's a button with text-red-400 class (trash icon)
    const deleteBtn = cardContainer.locator('button.text-red-400, button:has(.text-red-400)').first();
    await deleteBtn.click({ force: true });
    
    // Wait for delete to complete
    const deleteResponse = await deletePromise;
    expect(deleteResponse.status()).toBe(200);
    
    // Route should be removed from list
    createdRouteId = null; // Already deleted
    
    // Verify the route is gone from API
    const routesAfter = await request.get(`${baseUrl}/api/routes`);
    const afterList = await routesAfter.json();
    const notFound = afterList.find((r: any) => r.id === testRoute.id);
    expect(notFound).toBeUndefined();
  });

  test('UI: Route export buttons visible on hover (NEW v2.2)', async ({ page, request }) => {
    // Create a test route first
    const testRoute = {
      id: `test_export_ui_${Date.now()}`,
      name: `TEST_Export_UI_${Date.now()}`,
      points: [{ x: 0, y: 0, z: 5, yaw: 0, timestamp: 0, is_keyframe: true }],
      keyframes: [{ x: 0, y: 0, z: 5, yaw: 0, timestamp: 0, is_keyframe: true }],
      total_distance: 50.0,
      created_at: new Date().toISOString()
    };
    await request.post(`${baseUrl}/api/routes`, { data: testRoute });
    createdRouteId = testRoute.id;

    // Navigate to history page
    await page.goto('/', { waitUntil: 'domcontentloaded' });
    await waitForAppReady(page);
    await navigateToTab(page, 'tab-history');
    
    // Wait for routes to load
    const routeHistory = page.getByTestId('route-history');
    await expect(routeHistory).toBeVisible({ timeout: 10000 });
    
    // Find the route card containing our route name
    const routeCard = page.locator(`h3:has-text("${testRoute.name}")`).first();
    await expect(routeCard).toBeVisible();
    
    // Hover over the card to show export buttons
    const cardContainer = routeCard.locator('..').locator('..');
    await cardContainer.hover();
    
    // Check export JSON button is visible with correct data-testid
    const exportJsonBtn = page.getByTestId(`export-json-${testRoute.id}`);
    await expect(exportJsonBtn).toBeVisible({ timeout: 5000 });
    
    // Check export KML button is visible with correct data-testid
    const exportKmlBtn = page.getByTestId(`export-kml-${testRoute.id}`);
    await expect(exportKmlBtn).toBeVisible();
  });

  test('API: Export route as JSON via API (NEW v2.2)', async ({ request }) => {
    // Create a test route first
    const testRoute = {
      id: `test_api_export_json_${Date.now()}`,
      name: `TEST_API_Export_JSON_${Date.now()}`,
      points: [
        { x: 0, y: 0, z: 5, yaw: 0, timestamp: 0, is_keyframe: true },
        { x: 10, y: 10, z: 10, yaw: 0.5, timestamp: 1, is_keyframe: false }
      ],
      keyframes: [{ x: 0, y: 0, z: 5, yaw: 0, timestamp: 0, is_keyframe: true }],
      total_distance: 14.14,
      created_at: new Date().toISOString()
    };
    
    // Create route
    await request.post(`${baseUrl}/api/routes`, { data: testRoute });
    createdRouteId = testRoute.id;

    try {
      // Export as JSON
      const response = await request.get(`${baseUrl}/api/routes/${testRoute.id}/export/json`);
      expect(response.status()).toBe(200);
      
      const data = await response.json();
      expect(data.id).toBe(testRoute.id);
      expect(data.name).toBe(testRoute.name);
      expect(data.points.length).toBe(2);
      expect(data.keyframes.length).toBe(1);
    } finally {
      // Cleanup
      await request.delete(`${baseUrl}/api/routes/${testRoute.id}`);
      createdRouteId = null;
    }
  });

  test('API: Export route as KML returns valid KML (NEW v2.2)', async ({ request }) => {
    // Create a test route first
    const testRoute = {
      id: `test_api_export_kml_${Date.now()}`,
      name: `TEST_API_Export_KML_${Date.now()}`,
      points: [
        { x: 0, y: 0, z: 5, yaw: 0, timestamp: 0, is_keyframe: true },
        { x: 10, y: 10, z: 10, yaw: 0.5, timestamp: 1, is_keyframe: false }
      ],
      keyframes: [{ x: 0, y: 0, z: 5, yaw: 0, timestamp: 0, is_keyframe: true }],
      total_distance: 14.14,
      created_at: new Date().toISOString()
    };
    
    // Create route
    await request.post(`${baseUrl}/api/routes`, { data: testRoute });
    createdRouteId = testRoute.id;

    try {
      // Export as KML
      const response = await request.get(`${baseUrl}/api/routes/${testRoute.id}/export/kml`);
      expect(response.status()).toBe(200);
      
      const kmlContent = await response.text();
      
      // Verify KML structure
      expect(kmlContent).toContain('<?xml version="1.0"');
      expect(kmlContent).toContain('<kml xmlns="http://www.opengis.net/kml/2.2">');
      expect(kmlContent).toContain(`<name>${testRoute.name}</name>`);
      expect(kmlContent).toContain('<coordinates>');
      expect(kmlContent).toContain('</coordinates>');
      expect(kmlContent).toContain('<LineString>');
      expect(kmlContent).toContain('Visual Homing Route Export');
    } finally {
      // Cleanup
      await request.delete(`${baseUrl}/api/routes/${testRoute.id}`);
      createdRouteId = null;
    }
  });
});
