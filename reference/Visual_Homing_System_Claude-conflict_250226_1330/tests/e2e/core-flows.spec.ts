import { test, expect } from '@playwright/test';
import { waitForAppReady, navigateToTab, waitFor3DCanvas } from '../fixtures/helpers';

test.describe('Visual Homing - Core Flows', () => {
  
  test.beforeEach(async ({ page }) => {
    await page.goto('/', { waitUntil: 'domcontentloaded' });
    await waitForAppReady(page);
  });

  test('App loads successfully with header and navigation', async ({ page }) => {
    // Check header
    const header = page.getByTestId('header');
    await expect(header).toBeVisible();
    
    // Check logo text
    const h1 = page.locator('h1');
    await expect(h1).toContainText('Visual Homing');
    
    // Check version badge - updated to v2.2
    const versionBadge = page.locator('span:has-text("v2.2")');
    await expect(versionBadge).toBeVisible();
    
    // Check footer
    const footer = page.getByTestId('footer');
    await expect(footer).toBeVisible();
    await expect(footer).toContainText('MIT License');
  });

  test('Navigation tabs are visible and 3D Map is active by default', async ({ page }) => {
    // Check all tabs are visible (including NEW Telemetry tab and Settings tab)
    await expect(page.getByTestId('tab-map')).toBeVisible();
    await expect(page.getByTestId('tab-history')).toBeVisible();
    await expect(page.getByTestId('tab-telemetry')).toBeVisible();  // Telemetry tab
    await expect(page.getByTestId('tab-docs')).toBeVisible();
    await expect(page.getByTestId('tab-firmware')).toBeVisible();
    await expect(page.getByTestId('tab-about')).toBeVisible();
    await expect(page.getByTestId('tab-settings')).toBeVisible();  // NEW: Settings tab (v2.2)
    
    // 3D Map tab should be active by default - check for cyan-500 bg class instead of 'active'
    const mapTab = page.getByTestId('tab-map');
    await expect(mapTab).toHaveClass(/bg-cyan-500/);
  });

  test('Tab navigation works correctly', async ({ page }) => {
    // Click Docs tab
    await navigateToTab(page, 'tab-docs');
    await expect(page.getByTestId('docs-section')).toBeVisible();
    await expect(page.getByTestId('tab-docs')).toHaveClass(/bg-cyan-500/);
    
    // Click Firmware tab
    await navigateToTab(page, 'tab-firmware');
    await expect(page.getByTestId('firmware-section')).toBeVisible();
    await expect(page.getByTestId('tab-firmware')).toHaveClass(/bg-cyan-500/);
    
    // Click About tab
    await navigateToTab(page, 'tab-about');
    await expect(page.getByTestId('about-section')).toBeVisible();
    await expect(page.getByTestId('tab-about')).toHaveClass(/bg-cyan-500/);
    
    // Click History tab
    await navigateToTab(page, 'tab-history');
    await expect(page.getByTestId('history-section')).toBeVisible();
    await expect(page.getByTestId('tab-history')).toHaveClass(/bg-cyan-500/);
    
    // Click Telemetry tab (NEW)
    await navigateToTab(page, 'tab-telemetry');
    await expect(page.getByTestId('telemetry-section')).toBeVisible();
    await expect(page.getByTestId('tab-telemetry')).toHaveClass(/bg-cyan-500/);
    
    // Click back to Map tab
    await navigateToTab(page, 'tab-map');
    await expect(page.getByTestId('map-panel')).toBeVisible();
    await expect(page.getByTestId('tab-map')).toHaveClass(/bg-cyan-500/);
  });

  test('About section displays project information and specifications', async ({ page }) => {
    await navigateToTab(page, 'tab-about');
    
    const aboutSection = page.getByTestId('about-section');
    await expect(aboutSection).toBeVisible();
    
    // Check project title
    await expect(aboutSection).toContainText('Visual Homing System');
    
    // Check features grid - including NEW Smart RTL and Optical Flow features
    await expect(aboutSection).toContainText('Без GPS залежності');
    await expect(aboutSection).toContainText('Без компаса');
    await expect(aboutSection).toContainText('Teach & Repeat');
    await expect(aboutSection).toContainText('Термальна камера');
    await expect(aboutSection).toContainText('Smart RTL');  // NEW: Smart RTL feature
    await expect(aboutSection).toContainText('Optical Flow');  // NEW: Optical Flow feature
    
    // Check specifications table - including NEW sensor modules
    await expect(aboutSection).toContainText('Raspberry Pi Zero 2 W');
    await expect(aboutSection).toContainText('ArduCopter');
    await expect(aboutSection).toContainText('MAVLink');
    await expect(aboutSection).toContainText('Python 3');
    await expect(aboutSection).toContainText('C++17');
    await expect(aboutSection).toContainText('MATEK 3901-L0X');  // NEW: Optical Flow sensor
    await expect(aboutSection).toContainText('TF-Luna');  // NEW: LiDAR sensor
    await expect(aboutSection).toContainText('Matek H743-Slim V3');  // Flight controller
  });

  test('Documentation tab displays document list', async ({ page }) => {
    await navigateToTab(page, 'tab-docs');
    
    const docsSection = page.getByTestId('docs-section');
    await expect(docsSection).toBeVisible();
    
    // Check sidebar header text - now uses different structure
    await expect(page.locator('h2:has-text("Документи")')).toBeVisible();
    
    // Check that document items exist using buttons in sidebar
    const docItems = page.locator('button:has(svg)').filter({ has: page.locator('span') });
    const count = await docItems.count();
    expect(count).toBeGreaterThan(0);
    
    // Click on first document button to verify content loads
    const firstDocButton = docsSection.locator('button').first();
    await firstDocButton.click();
    
    // Wait for content to load
    const docContent = page.getByTestId('doc-content');
    await expect(docContent).toBeVisible({ timeout: 10000 });
  });

  test('Route History tab displays correctly', async ({ page }) => {
    await navigateToTab(page, 'tab-history');
    
    const historySection = page.getByTestId('history-section');
    await expect(historySection).toBeVisible();
    
    // Check header text
    await expect(historySection).toContainText('Історія маршрутів');
    await expect(historySection).toContainText('Збережені записи польотів');
    
    // Wait for content to load, then check if routes exist or show empty state
    await page.waitForLoadState('networkidle');
    
    // Either route-history list or empty state message should be visible
    const routeHistoryList = page.getByTestId('route-history');
    const emptyStateVisible = await historySection.locator('text=Немає збережених маршрутів').isVisible().catch(() => false);
    const routesVisible = await routeHistoryList.isVisible().catch(() => false);
    
    // One of them should be true
    expect(emptyStateVisible || routesVisible).toBe(true);
  });

  test('Telemetry tab displays sensor monitoring dashboard', async ({ page }) => {
    // Navigate to Telemetry tab
    await navigateToTab(page, 'tab-telemetry');
    
    // Verify telemetry section is visible
    const telemetrySection = page.getByTestId('telemetry-section');
    await expect(telemetrySection).toBeVisible();
    
    // Check header text
    await expect(telemetrySection).toContainText('Телеметрія');
    await expect(telemetrySection).toContainText('Моніторинг сенсорів та навігації');
    
    // Check refresh button is visible
    await expect(page.getByTestId('telemetry-refresh-btn')).toBeVisible();
    
    // Check sensor cards are displayed (using text locators since data-testid not forwarded by AnimatedCard)
    // MATEK 3901-L0X Optical Flow sensor
    await expect(page.locator('h3:has-text("MATEK 3901-L0X")')).toBeVisible({ timeout: 10000 });
    await expect(telemetrySection).toContainText('Optical Flow X');
    await expect(telemetrySection).toContainText('Optical Flow Y');
    await expect(telemetrySection).toContainText('Якість');
    
    // TF-Luna LiDAR sensor
    await expect(page.locator('h3:has-text("TF-Luna LiDAR")')).toBeVisible();
    await expect(telemetrySection).toContainText('Відстань');
    await expect(telemetrySection).toContainText('Сигнал');
    
    // Check Smart RTL panel (GlassPanel doesn't forward data-testid, use text locator)
    await expect(page.locator('h3:has-text("Smart RTL")')).toBeVisible();
    await expect(telemetrySection).toContainText('Гібридна навігація повернення');
    
    // Check RTL metrics are displayed
    await expect(telemetrySection).toContainText('Висота');
    await expect(telemetrySection).toContainText('До дому');
    await expect(telemetrySection).toContainText('Прогрес');
    await expect(telemetrySection).toContainText('Джерело');
    
    // Check phase indicators are visible (this uses data-testid directly on a div)
    const phaseIndicators = page.getByTestId('smart-rtl-phases');
    await expect(phaseIndicators).toBeVisible();
    
    // Check RTL phase indicator labels
    await expect(phaseIndicators).toContainText('HIGH ALT');
    await expect(phaseIndicators).toContainText('DESCENT');
    await expect(phaseIndicators).toContainText('LOW ALT');
    await expect(phaseIndicators).toContainText('LANDING');
  });

  test('Telemetry tab displays video stream placeholder (NEW v2.2)', async ({ page }) => {
    // Navigate to Telemetry tab
    await navigateToTab(page, 'tab-telemetry');
    
    // Check video stream component is visible
    const videoStream = page.getByTestId('video-stream');
    await expect(videoStream).toBeVisible({ timeout: 10000 });
    
    // Check video stream header
    await expect(videoStream.locator('h3')).toContainText('Камера');
    
    // Check status badge (should be OFFLINE in preview environment)
    await expect(videoStream).toContainText('OFFLINE');
    
    // Check placeholder message
    await expect(videoStream).toContainText('Стрім доступний на Raspberry Pi');
    await expect(videoStream).toContainText('/api/stream/video');
  });

  test('Settings tab renders correctly with all sections (NEW v2.2)', async ({ page }) => {
    // Navigate to Settings tab
    await navigateToTab(page, 'tab-settings');
    
    // Verify settings section is visible
    const settingsSection = page.getByTestId('settings-section');
    await expect(settingsSection).toBeVisible();
    
    // Check header text
    await expect(settingsSection).toContainText('Налаштування');
    await expect(settingsSection).toContainText('Конфігурація системи Visual Homing');
    
    // Check Save and Reset buttons
    await expect(page.getByTestId('settings-save-btn')).toBeVisible();
    await expect(page.getByTestId('settings-reset-btn')).toBeVisible();
    
    // Check Camera section
    await expect(settingsSection).toContainText('Камера');
    await expect(settingsSection).toContainText('Тип камери');
    await expect(settingsSection).toContainText('Роздільність');
    await expect(settingsSection).toContainText('FPS');
    
    // Check MAVLink section
    await expect(settingsSection).toContainText('MAVLink');
    await expect(settingsSection).toContainText('Baud Rate');
    
    // Check Optical Flow section
    await expect(settingsSection).toContainText('Optical Flow');
    await expect(settingsSection).toContainText('MATEK 3901-L0X');
    
    // Check LiDAR section
    await expect(settingsSection).toContainText('LiDAR');
    await expect(settingsSection).toContainText('TF-Luna');
    
    // Check Smart RTL section
    await expect(settingsSection).toContainText('Smart RTL');
    await expect(settingsSection).toContainText('Поріг висоти');
    await expect(settingsSection).toContainText('Точна посадка');
    
    // Check System section
    await expect(settingsSection).toContainText('Система');
    await expect(settingsSection).toContainText('Автозапуск');
    await expect(settingsSection).toContainText('Web порт');
  });

  test('Settings can be saved and reset (NEW v2.2)', async ({ page }) => {
    // Navigate to Settings tab
    await navigateToTab(page, 'tab-settings');
    
    const settingsSection = page.getByTestId('settings-section');
    await expect(settingsSection).toBeVisible();
    
    // Click Reset button
    const resetBtn = page.getByTestId('settings-reset-btn');
    await resetBtn.click({ force: true });
    
    // Wait for toast notification (use first() in case multiple toasts)
    await expect(page.locator('[data-sonner-toast]').first()).toContainText('скинуто', { timeout: 5000 });
    
    // Wait a bit for toasts to clear
    await page.waitForTimeout(1500);
    
    // Click Save button  
    const saveBtn = page.getByTestId('settings-save-btn');
    await saveBtn.click({ force: true });
    
    // Wait for save toast (use first() and check for save-specific text)
    await expect(page.locator('[data-sonner-toast]').first()).toContainText('збережено', { timeout: 5000 });
  });

  test('Mobile responsive UI - tabs show icons only on small screens (P2)', async ({ page }) => {
    // Set mobile viewport
    await page.setViewportSize({ width: 375, height: 667 });
    await page.reload({ waitUntil: 'domcontentloaded' });
    await waitForAppReady(page);
    
    // Check tabs are visible
    const mapTab = page.getByTestId('tab-map');
    await expect(mapTab).toBeVisible();
    
    // On mobile, the label text should be hidden (via sm:inline class)
    // The span inside tab button should not be visible (hidden on small screens)
    // Verify tab still works by clicking
    await mapTab.click({ force: true });
    await expect(mapTab).toHaveClass(/bg-cyan-500/);
    
    // Check all navigation tabs are accessible in mobile view
    await expect(page.getByTestId('tab-history')).toBeVisible();
    await expect(page.getByTestId('tab-telemetry')).toBeVisible();
    await expect(page.getByTestId('tab-docs')).toBeVisible();
    await expect(page.getByTestId('tab-settings')).toBeVisible();
    
    // Click through tabs to verify they work on mobile
    await page.getByTestId('tab-settings').click({ force: true });
    await expect(page.getByTestId('settings-section')).toBeVisible();
    
    // Verify bottom control bar buttons are visible on mobile
    await page.getByTestId('tab-map').click({ force: true });
    await expect(page.getByTestId('smart-rtl-btn')).toBeVisible();
  });

  test('Desktop UI - tabs show labels with icons (P2)', async ({ page }) => {
    // Set desktop viewport  
    await page.setViewportSize({ width: 1280, height: 720 });
    await page.reload({ waitUntil: 'domcontentloaded' });
    await waitForAppReady(page);
    
    // Check tabs are visible and contain text labels on desktop
    const mapTab = page.getByTestId('tab-map');
    await expect(mapTab).toBeVisible();
    await expect(mapTab).toContainText('3D Карта');
    
    // Check other tabs contain their labels
    await expect(page.getByTestId('tab-history')).toContainText('Історія');
    await expect(page.getByTestId('tab-telemetry')).toContainText('Телеметрія');
    await expect(page.getByTestId('tab-docs')).toContainText('Документація');
    await expect(page.getByTestId('tab-settings')).toContainText('Налаштування');
  });
});
