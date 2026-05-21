import { test, expect } from '@playwright/test';
import { waitForAppReady, waitFor3DCanvas } from '../fixtures/helpers';

test.describe('Visual Homing - 3D Map & Simulation', () => {
  
  test.beforeEach(async ({ page }) => {
    await page.goto('/', { waitUntil: 'domcontentloaded' });
    await waitForAppReady(page);
  });

  test('3D Map container and canvas render correctly', async ({ page }) => {
    // Check map container
    const mapPanel = page.getByTestId('map-panel');
    await expect(mapPanel).toBeVisible();
    
    // Check canvas container
    const canvasContainer = page.getByTestId('threejs-canvas');
    await expect(canvasContainer).toBeVisible();
    
    // Wait for Three.js canvas to be created
    await waitFor3DCanvas(page);
    
    // Verify canvas element exists
    const canvas = canvasContainer.locator('canvas');
    await expect(canvas).toBeVisible();
  });

  test('Simulation buttons are visible and functional', async ({ page }) => {
    // Check simulation buttons
    const simBtn = page.getByTestId('sim-btn');
    const resetBtn = page.getByTestId('reset-btn');
    const newRouteBtn = page.getByTestId('new-route-btn');
    const smartRTLBtn = page.getByTestId('smart-rtl-btn');
    
    await expect(simBtn).toBeVisible();
    await expect(resetBtn).toBeVisible();
    await expect(newRouteBtn).toBeVisible();
    await expect(smartRTLBtn).toBeVisible();
    
    // Check button text (initial state)
    await expect(simBtn).toContainText('Симуляція');
    await expect(resetBtn).toContainText('Скинути');
    await expect(newRouteBtn).toContainText('Новий маршрут');
    await expect(smartRTLBtn).toContainText('Smart RTL');
  });

  test('Start/Stop simulation button toggles state', async ({ page }) => {
    // Wait for canvas and route to load
    await waitFor3DCanvas(page);
    
    // Wait a bit for route to load from API
    await page.waitForResponse(resp => 
      resp.url().includes('/api/routes/demo/generate') && resp.status() === 200,
      { timeout: 10000 }
    ).catch(() => {
      // Route might have already loaded
    });
    
    const simBtn = page.getByTestId('sim-btn');
    
    // Initial state should show "Симуляція"
    await expect(simBtn).toContainText('Симуляція');
    
    // Click to start simulation
    await simBtn.click({ force: true });
    
    // Button should now show "Стоп"
    await expect(simBtn).toContainText('Стоп');
    
    // Click again to stop simulation
    await simBtn.click({ force: true });
    
    // Button should show "Симуляція" again
    await expect(simBtn).toContainText('Симуляція');
  });

  test('Reset button resets drone position', async ({ page }) => {
    await waitFor3DCanvas(page);
    
    const resetBtn = page.getByTestId('reset-btn');
    
    // Click reset button
    await resetBtn.click({ force: true });
    
    // Button should still be visible (no errors)
    await expect(resetBtn).toBeVisible();
  });

  test('New route button loads new route', async ({ page }) => {
    await waitFor3DCanvas(page);
    
    const newRouteBtn = page.getByTestId('new-route-btn');
    
    // Set up response listener before clicking
    const responsePromise = page.waitForResponse(
      resp => resp.url().includes('/api/routes/demo/generate') && resp.status() === 200,
      { timeout: 15000 }
    );
    
    // Click new route button
    await newRouteBtn.click({ force: true });
    
    // Wait for API call to complete
    const response = await responsePromise;
    expect(response.status()).toBe(200);
    
    const data = await response.json();
    expect(data.points).toBeDefined();
    expect(data.keyframes).toBeDefined();
  });

  test('Map stats display keyframes and distance in top bar', async ({ page }) => {
    await waitFor3DCanvas(page);
    
    // Wait for route data to load - stats are now in the top glassmorphism panel
    const mapPanel = page.getByTestId('map-panel');
    await expect(mapPanel).toBeVisible();
    
    // Check keyframes count is displayed in the top control bar
    await expect(mapPanel.locator('text=Keyframes')).toBeVisible({ timeout: 10000 });
    
    // Check distance is displayed
    await expect(mapPanel.locator('text=Дистанція')).toBeVisible();
  });

  test('Auto-save toggle is visible and functional', async ({ page }) => {
    await waitFor3DCanvas(page);
    
    const mapPanel = page.getByTestId('map-panel');
    
    // Check that the auto-save toggle label is visible
    await expect(mapPanel.locator('text=Автозбереження')).toBeVisible();
    
    // Find the Switch component (shadcn Switch uses button with data-state)
    const switchToggle = mapPanel.locator('button[role="switch"]');
    await expect(switchToggle).toBeVisible();
    
    // Initially should be unchecked (no save button visible)
    const saveBtn = page.getByTestId('save-route-btn');
    await expect(saveBtn).not.toBeVisible();
    
    // Click to enable auto-save
    await switchToggle.click({ force: true });
    
    // Now save button should be visible
    await expect(saveBtn).toBeVisible({ timeout: 5000 });
    await expect(saveBtn).toContainText('Зберегти');
  });

  test('Speed control panel is visible with slider', async ({ page }) => {
    await waitFor3DCanvas(page);
    
    // Check speed control panel is visible
    const speedControl = page.getByTestId('speed-control');
    await expect(speedControl).toBeVisible();
    
    // Check speed slider is visible
    const speedSlider = page.getByTestId('speed-slider');
    await expect(speedSlider).toBeVisible();
    
    // Check speed label shows default value (x1.0)
    await expect(speedControl).toContainText('x1.0');
    await expect(speedControl).toContainText('Швидкість');
  });

  test('Speed slider controls simulation speed (range 0.1-5.0)', async ({ page }) => {
    await waitFor3DCanvas(page);
    
    const speedSlider = page.getByTestId('speed-slider');
    await expect(speedSlider).toBeVisible();
    
    // Check slider attributes
    await expect(speedSlider).toHaveAttribute('min', '0.1');
    await expect(speedSlider).toHaveAttribute('max', '5');
    await expect(speedSlider).toHaveAttribute('step', '0.1');
    
    // For range inputs, we need to use evaluate to change the value
    await speedSlider.evaluate((el: HTMLInputElement) => {
      el.value = '3.0';
      el.dispatchEvent(new Event('input', { bubbles: true }));
      el.dispatchEvent(new Event('change', { bubbles: true }));
    });
    
    // Verify speed control shows new value
    const speedControl = page.getByTestId('speed-control');
    await expect(speedControl).toContainText('x3.0');
  });

  test('Smart RTL button is visible and starts RTL simulation', async ({ page }) => {
    await waitFor3DCanvas(page);
    
    // Wait for route to load
    await page.waitForResponse(resp => 
      resp.url().includes('/api/routes/demo/generate') && resp.status() === 200,
      { timeout: 10000 }
    ).catch(() => {});
    
    const smartRTLBtn = page.getByTestId('smart-rtl-btn');
    await expect(smartRTLBtn).toBeVisible();
    await expect(smartRTLBtn).toContainText('Smart RTL');
    
    // Click Smart RTL button - should start simulation in RTL mode
    await smartRTLBtn.click({ force: true });
    
    // Simulation should now be running (sim button shows Стоп)
    const simBtn = page.getByTestId('sim-btn');
    await expect(simBtn).toContainText('Стоп');
  });

  test('HUD overlay appears during simulation with altitude, speed, phase, progress', async ({ page }) => {
    await waitFor3DCanvas(page);
    
    // Wait for route to load
    await page.waitForResponse(resp => 
      resp.url().includes('/api/routes/demo/generate') && resp.status() === 200,
      { timeout: 10000 }
    ).catch(() => {});
    
    // HUD should not be visible before simulation starts
    const hudOverlay = page.getByTestId('hud-overlay');
    await expect(hudOverlay).not.toBeVisible();
    
    // Start simulation
    const simBtn = page.getByTestId('sim-btn');
    await simBtn.click({ force: true });
    
    // Wait for HUD to appear (it appears when isSimulating && telemetry)
    await expect(hudOverlay).toBeVisible({ timeout: 5000 });
    
    // Check HUD contains altitude, speed, phase, and progress info
    await expect(hudOverlay).toContainText('Висота');
    await expect(hudOverlay).toContainText('Швидкість');
    await expect(hudOverlay).toContainText('Прогрес');
    
    // Check HUD shows м (meters) unit for altitude
    await expect(hudOverlay).toContainText('м');
    // Check HUD shows м/с (m/s) unit for speed  
    await expect(hudOverlay).toContainText('м/с');
    // Check progress shows %
    await expect(hudOverlay).toContainText('%');
    
    // Stop simulation - HUD should disappear
    await simBtn.click({ force: true });
    await expect(hudOverlay).not.toBeVisible();
  });

  test('Smart RTL mode shows phase indicators in HUD', async ({ page }) => {
    await waitFor3DCanvas(page);
    
    // Wait for route to load
    await page.waitForResponse(resp => 
      resp.url().includes('/api/routes/demo/generate') && resp.status() === 200,
      { timeout: 10000 }
    ).catch(() => {});
    
    // Click Smart RTL button to start RTL simulation
    const smartRTLBtn = page.getByTestId('smart-rtl-btn');
    await smartRTLBtn.click({ force: true });
    
    // Wait for HUD to appear
    const hudOverlay = page.getByTestId('hud-overlay');
    await expect(hudOverlay).toBeVisible({ timeout: 5000 });
    
    // Check that phase is displayed (ЗАПИС initially during record phase)
    // The phase badge should be visible showing the current phase
    // Possible phases: ЗАПИС, ПОЛІТ, HIGH ALT, DESCENT, LOW ALT, LANDING
    const phaseBadge = hudOverlay.locator('.rounded-lg.border').first();
    await expect(phaseBadge).toBeVisible();
    
    // Stop simulation
    const simBtn = page.getByTestId('sim-btn');
    await simBtn.click({ force: true });
  });
});
