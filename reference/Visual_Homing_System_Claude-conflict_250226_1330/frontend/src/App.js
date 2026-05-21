import { useState, useEffect, useCallback, useRef } from "react";
import axios from "axios";
import { Map, FileText, Code, Info, History, Play, Square, RotateCcw, Plus, Trash2, Save, X, Plane, Download, Activity, Radio, ArrowDown, Navigation, Wifi, WifiOff, Target, Settings, Video, FileDown } from "lucide-react";
import SimpleMap3D from "./components/SimpleMap3D";
import { Toaster, toast } from "sonner";
import { Switch } from "@/components/ui/switch";

const BACKEND_URL = process.env.REACT_APP_BACKEND_URL;
const API = `${BACKEND_URL}/api`;

// Glass Panel Component
const GlassPanel = ({ children, className = "", ...props }) => (
  <div className={`backdrop-blur-xl bg-black/60 border border-white/10 rounded-xl shadow-2xl ${className}`} {...props}>
    {children}
  </div>
);

// Animated Card Component  
const AnimatedCard = ({ children, className = "", delay = 0, ...props }) => (
  <div 
    className={`animate-slide-up bg-zinc-900/80 backdrop-blur-md border border-white/10 rounded-xl 
                hover:border-cyan-500/50 hover:shadow-glow-sm transition-all duration-300 ${className}`}
    style={{ animationDelay: `${delay}ms` }}
    {...props}
  >
    {children}
  </div>
);

// Navigation Tab Button
const TabButton = ({ active, icon: Icon, label, onClick, testId }) => (
  <button
    onClick={onClick}
    data-testid={testId}
    className={`flex items-center gap-1.5 px-3 py-2 md:px-4 md:py-2.5 rounded-lg text-xs md:text-sm font-medium transition-all duration-300 whitespace-nowrap shrink-0
                ${active 
                  ? 'bg-cyan-500 text-black shadow-glow' 
                  : 'text-zinc-400 hover:text-white hover:bg-white/5'}`}
  >
    <Icon size={16} className="md:w-[18px] md:h-[18px]" />
    <span className="hidden sm:inline">{label}</span>
  </button>
);

// Document Viewer
const DocViewer = ({ doc }) => {
  if (!doc) {
    return (
      <div className="flex flex-col items-center justify-center h-96 text-zinc-500 animate-fade-in">
        <FileText size={64} className="mb-4 opacity-30" />
        <p className="text-lg">Виберіть документ зі списку</p>
      </div>
    );
  }

  return (
    <div className="animate-fade-in p-6" data-testid="doc-content">
      <div 
        className="prose prose-invert prose-cyan max-w-none
                   prose-headings:font-heading prose-headings:text-white
                   prose-h1:text-3xl prose-h1:text-cyan-400 prose-h1:border-b prose-h1:border-zinc-800 prose-h1:pb-4
                   prose-h2:text-xl prose-h2:text-white/90 prose-h2:mt-8
                   prose-p:text-zinc-400 prose-p:leading-relaxed
                   prose-code:bg-zinc-800 prose-code:px-2 prose-code:py-1 prose-code:rounded prose-code:text-cyan-400
                   prose-pre:bg-zinc-900 prose-pre:border prose-pre:border-zinc-800
                   prose-a:text-cyan-400 prose-a:no-underline hover:prose-a:underline
                   prose-strong:text-white prose-li:text-zinc-400
                   prose-img:rounded-xl prose-img:border prose-img:border-zinc-700 prose-img:max-w-full prose-img:my-4"
        dangerouslySetInnerHTML={{ __html: doc.html }}
      />
    </div>
  );
};

// File Tree Component
const FileTree = ({ files, onSelect, selectedPath }) => {
  const grouped = {};
  files.forEach(f => {
    const parts = f.split('/');
    const folder = parts.length > 1 ? parts[0] : 'root';
    if (!grouped[folder]) grouped[folder] = [];
    grouped[folder].push(f);
  });

  const getIcon = (path) => {
    if (path.endsWith('.py')) return '🐍';
    if (path.endsWith('.cpp') || path.endsWith('.hpp')) return '⚙️';
    if (path.endsWith('.sh')) return '📜';
    return '📄';
  };

  return (
    <div className="space-y-4" data-testid="file-tree">
      {Object.entries(grouped).map(([folder, items], idx) => (
        <div key={folder} className="animate-slide-up" style={{ animationDelay: `${idx * 100}ms` }}>
          <div className="text-xs font-mono text-cyan-500 uppercase tracking-wider mb-2 flex items-center gap-2">
            <Code size={14} />
            {folder}
          </div>
          <div className="space-y-1">
            {items.map(item => (
              <button
                key={item}
                onClick={() => onSelect(item)}
                className={`w-full text-left px-3 py-2 rounded-lg text-sm transition-all duration-200
                           ${selectedPath === item 
                             ? 'bg-cyan-950/50 border border-cyan-500/30 text-cyan-400' 
                             : 'text-zinc-400 hover:text-white hover:bg-white/5'}`}
              >
                <span className="mr-2">{getIcon(item)}</span>
                {item.split('/').pop()}
              </button>
            ))}
          </div>
        </div>
      ))}
    </div>
  );
};

// Code Viewer Component
const CodeViewer = ({ code, path }) => {
  if (!code) {
    return (
      <div className="flex flex-col items-center justify-center h-96 text-zinc-500 animate-fade-in">
        <Code size={64} className="mb-4 opacity-30" />
        <p className="text-lg">Виберіть файл для перегляду</p>
      </div>
    );
  }

  return (
    <div className="animate-fade-in" data-testid="code-viewer">
      <div className="flex items-center justify-between px-4 py-3 border-b border-zinc-800 bg-zinc-900/50">
        <span className="font-mono text-sm text-cyan-400">{path}</span>
      </div>
      <pre className="p-4 overflow-x-auto text-sm">
        <code className="font-mono text-zinc-300 leading-relaxed">{code}</code>
      </pre>
    </div>
  );
};

// Route History Component
const RouteHistory = ({ routes, onSelect, onDelete, loading }) => {
  if (loading) {
    return (
      <div className="flex items-center justify-center h-64">
        <div className="w-8 h-8 border-2 border-cyan-500 border-t-transparent rounded-full animate-spin" />
      </div>
    );
  }

  if (!routes || routes.length === 0) {
    return (
      <div className="flex flex-col items-center justify-center h-64 text-zinc-500 animate-fade-in">
        <History size={64} className="mb-4 opacity-30" />
        <p className="text-lg mb-2">Немає збережених маршрутів</p>
        <p className="text-sm text-zinc-600">Маршрути з'являться тут після збереження</p>
      </div>
    );
  }

  return (
    <div className="space-y-3 animate-fade-in" data-testid="route-history">
      {routes.map((route, idx) => (
        <AnimatedCard key={route.id} delay={idx * 50} className="p-4 group cursor-pointer">
          <div className="flex items-center justify-between">
            <div className="flex-1" onClick={() => onSelect(route)}>
              <h3 className="font-heading font-bold text-white group-hover:text-cyan-400 transition-colors">
                {route.name}
              </h3>
              <div className="flex items-center gap-4 mt-2 text-xs text-zinc-500">
                <span className="font-mono">{route.keyframes?.length || 0} keyframes</span>
                <span className="font-mono">{route.total_distance?.toFixed(1) || 0}m</span>
                <span>{new Date(route.created_at).toLocaleDateString('uk-UA')}</span>
              </div>
            </div>
            <div className="flex items-center gap-2 opacity-0 group-hover:opacity-100 transition-opacity">
              <button 
                onClick={(e) => { e.stopPropagation(); window.open(`${API}/routes/${route.id}/export/json`, '_blank'); }}
                className="p-2 rounded-lg hover:bg-emerald-500/20 text-emerald-400 transition-colors"
                title="Експорт JSON"
                data-testid={`export-json-${route.id}`}
              >
                <FileDown size={18} />
              </button>
              <button 
                onClick={(e) => { e.stopPropagation(); window.open(`${API}/routes/${route.id}/export/kml`, '_blank'); }}
                className="p-2 rounded-lg hover:bg-amber-500/20 text-amber-400 transition-colors"
                title="Експорт KML"
                data-testid={`export-kml-${route.id}`}
              >
                <Download size={18} />
              </button>
              <button 
                onClick={() => onSelect(route)}
                className="p-2 rounded-lg hover:bg-cyan-500/20 text-cyan-400 transition-colors"
                title="Переглянути"
              >
                <Map size={18} />
              </button>
              <button 
                onClick={(e) => { e.stopPropagation(); onDelete(route.id); }}
                className="p-2 rounded-lg hover:bg-red-500/20 text-red-400 transition-colors"
                title="Видалити"
              >
                <Trash2 size={18} />
              </button>
            </div>
          </div>
        </AnimatedCard>
      ))}
    </div>
  );
};

// Map Panel Component
const MapPanel = ({ onSaveRoute, saveEnabled, setSaveEnabled, selectedRoute }) => {
  const [route, setRoute] = useState(null);
  const initialRouteRef = useRef(selectedRoute);

  useEffect(() => {
    if (selectedRoute) setRoute(selectedRoute);
  }, [selectedRoute]);
  const [isSimulating, setIsSimulating] = useState(false);
  const [smartRTLMode, setSmartRTLMode] = useState(false);
  const [speedMultiplier, setSpeedMultiplier] = useState(1.0);
  const [telemetry, setTelemetry] = useState(null);
  const [stats, setStats] = useState({ keyframes: 0, distance: 0 });

  const loadDemoRoute = async () => {
    try {
      const response = await axios.get(`${API}/routes/demo/generate`);
      setRoute(response.data);
      setStats({
        keyframes: response.data.keyframes?.length || 0,
        distance: response.data.total_distance || 0
      });
    } catch (e) {
      console.error("Failed to load route:", e);
      toast.error("Помилка завантаження маршруту");
    }
  };

  useEffect(() => {
    if (initialRouteRef.current) {
      setRoute(initialRouteRef.current);
      setStats({
        keyframes: initialRouteRef.current.keyframes?.length || 0,
        distance: initialRouteRef.current.total_distance || 0
      });
    } else {
      loadDemoRoute();
    }
  }, []);

  const handleSaveRoute = async () => {
    if (!route) return;
    try {
      await onSaveRoute(route);
      toast.success("Маршрут збережено!");
    } catch (e) {
      toast.error("Помилка збереження");
    }
  };

  const handleSmartRTL = () => {
    if (!isSimulating) {
      setSmartRTLMode(true);
      setIsSimulating(true);
    } else {
      setSmartRTLMode(!smartRTLMode);
    }
  };

  const phaseLabels = {
    record: { label: 'ЗАПИС', color: 'text-cyan-400', bg: 'bg-cyan-500/20 border-cyan-500/30' },
    normal: { label: 'ПОЛІТ', color: 'text-cyan-400', bg: 'bg-cyan-500/20 border-cyan-500/30' },
    high_alt: { label: 'HIGH ALT', color: 'text-amber-400', bg: 'bg-amber-500/20 border-amber-500/30' },
    descent: { label: 'DESCENT', color: 'text-orange-400', bg: 'bg-orange-500/20 border-orange-500/30' },
    low_alt: { label: 'LOW ALT', color: 'text-purple-400', bg: 'bg-purple-500/20 border-purple-500/30' },
    precision_land: { label: 'LANDING', color: 'text-emerald-400', bg: 'bg-emerald-500/20 border-emerald-500/30' },
  };

  const currentPhase = phaseLabels[telemetry?.phase] || phaseLabels.normal;

  return (
    <div className="relative h-[calc(100vh-80px)] overflow-hidden" data-testid="map-panel">
      {/* 3D Canvas Background */}
      <div className="absolute inset-0 z-0">
        <SimpleMap3D 
          route={route} 
          isSimulating={isSimulating} 
          speedMultiplier={speedMultiplier}
          smartRTLMode={smartRTLMode}
          onTelemetryUpdate={setTelemetry}
        />
      </div>

      {/* Top Control Bar */}
      <GlassPanel className="absolute top-4 left-4 right-4 z-20 flex items-center justify-between px-6 py-3">
        <div className="flex items-center gap-6">
          <div className="flex items-center gap-3">
            <Plane size={20} className="text-cyan-400 animate-float" />
            <span className="font-heading font-bold text-white">3D Карта</span>
          </div>
          
          <div className="hidden md:flex items-center gap-4 text-sm">
            <div className="flex items-center gap-2 text-zinc-400">
              <span className="text-xs font-mono uppercase tracking-wider text-cyan-500">Keyframes:</span>
              <span className="font-mono font-bold text-white">{stats.keyframes}</span>
            </div>
            <div className="h-4 w-px bg-zinc-700" />
            <div className="flex items-center gap-2 text-zinc-400">
              <span className="text-xs font-mono uppercase tracking-wider text-cyan-500">Дистанція:</span>
              <span className="font-mono font-bold text-white">{stats.distance.toFixed(1)}m</span>
            </div>
          </div>
        </div>

        <div className="flex items-center gap-2">
          <div className="flex items-center gap-2 mr-4 text-sm">
            <Switch 
              checked={saveEnabled} 
              onCheckedChange={setSaveEnabled}
              className="data-[state=checked]:bg-cyan-500"
            />
            <span className="text-zinc-400 text-xs">Автозбереження</span>
          </div>
        </div>
      </GlassPanel>

      {/* HUD Overlay - Altitude, Speed, Phase */}
      {isSimulating && telemetry && (
        <div className="absolute top-20 right-4 z-20 space-y-2 animate-fade-in" data-testid="hud-overlay">
          {/* Phase Badge */}
          <div className={`px-4 py-2 rounded-lg border backdrop-blur-md font-mono text-sm font-bold text-center ${currentPhase.bg}`}>
            <span className={currentPhase.color}>{currentPhase.label}</span>
          </div>
          
          {/* Altitude */}
          <div className="bg-black/70 backdrop-blur-md rounded-lg border border-zinc-700/50 px-4 py-3 text-center min-w-[120px]">
            <div className="text-[10px] font-mono text-zinc-500 uppercase tracking-wider">Висота</div>
            <div className="font-mono font-bold text-2xl text-white">{telemetry.altitude}<span className="text-zinc-500 text-sm ml-1">м</span></div>
          </div>
          
          {/* Speed */}
          <div className="bg-black/70 backdrop-blur-md rounded-lg border border-zinc-700/50 px-4 py-3 text-center">
            <div className="text-[10px] font-mono text-zinc-500 uppercase tracking-wider">Швидкість</div>
            <div className="font-mono font-bold text-2xl text-white">{telemetry.speed}<span className="text-zinc-500 text-sm ml-1">м/с</span></div>
          </div>

          {/* Progress */}
          <div className="bg-black/70 backdrop-blur-md rounded-lg border border-zinc-700/50 px-4 py-3 text-center">
            <div className="text-[10px] font-mono text-zinc-500 uppercase tracking-wider">Прогрес</div>
            <div className="font-mono font-bold text-lg text-white">{telemetry.progress}<span className="text-zinc-500 text-sm ml-1">%</span></div>
            <div className="mt-1 h-1 bg-zinc-800 rounded-full overflow-hidden">
              <div 
                className={`h-full rounded-full transition-all duration-300 ${smartRTLMode ? 'bg-gradient-to-r from-cyan-500 via-amber-500 to-emerald-500' : 'bg-cyan-500'}`}
                style={{ width: `${telemetry.progress}%` }}
              />
            </div>
          </div>
        </div>
      )}

      {/* Speed Slider - Left side */}
      <div className="absolute top-20 left-4 z-20" data-testid="speed-control">
        <div className="bg-black/70 backdrop-blur-md rounded-lg border border-zinc-700/50 p-3 w-[140px]">
          <div className="text-[10px] font-mono text-zinc-500 uppercase tracking-wider mb-2 text-center">Швидкість</div>
          <input
            type="range"
            min="0.1"
            max="5"
            step="0.1"
            value={speedMultiplier}
            onChange={(e) => setSpeedMultiplier(parseFloat(e.target.value))}
            className="w-full h-1.5 bg-zinc-800 rounded-full appearance-none cursor-pointer
                       [&::-webkit-slider-thumb]:appearance-none [&::-webkit-slider-thumb]:h-4 [&::-webkit-slider-thumb]:w-4 
                       [&::-webkit-slider-thumb]:rounded-full [&::-webkit-slider-thumb]:bg-cyan-500 [&::-webkit-slider-thumb]:cursor-pointer
                       [&::-webkit-slider-thumb]:shadow-[0_0_8px_rgba(6,182,212,0.5)]"
            data-testid="speed-slider"
          />
          <div className="text-center font-mono font-bold text-white text-sm mt-1">x{speedMultiplier.toFixed(1)}</div>
        </div>
      </div>

      {/* Bottom Control Panel */}
      <GlassPanel className="absolute bottom-4 left-4 right-4 z-20 flex items-center justify-center gap-2 md:gap-3 px-3 md:px-6 py-3 md:py-4 overflow-x-auto scrollbar-hide"  data-testid="map-controls">
        <button
          onClick={() => { setIsSimulating(!isSimulating); if (isSimulating) { setSmartRTLMode(false); setTelemetry(null); } }}
          className={`flex items-center gap-1.5 px-3 py-2.5 md:px-5 md:py-3 rounded-lg font-bold transition-all duration-300 text-sm shrink-0
                     ${isSimulating 
                       ? 'bg-red-500/20 text-red-400 border border-red-500/30 hover:bg-red-500/30' 
                       : 'bg-cyan-500 text-black shadow-glow hover:scale-105'}`}
          data-testid="sim-btn"
        >
          {isSimulating ? <Square size={18} /> : <Play size={18} />}
          {isSimulating ? 'Стоп' : 'Симуляція'}
        </button>

        <button
          onClick={handleSmartRTL}
          className={`flex items-center gap-1.5 px-3 py-2.5 md:px-5 md:py-3 rounded-lg font-bold transition-all duration-300 text-sm shrink-0
                     ${smartRTLMode && isSimulating
                       ? 'bg-amber-500/20 text-amber-400 border border-amber-500/30 hover:bg-amber-500/30' 
                       : 'bg-gradient-to-r from-amber-500/20 to-emerald-500/20 text-amber-300 border border-amber-500/20 hover:border-amber-500/40'}`}
          data-testid="smart-rtl-btn"
        >
          <Navigation size={18} />
          Smart RTL
        </button>

        <button
          onClick={() => { setIsSimulating(false); setSmartRTLMode(false); setTelemetry(null); loadDemoRoute(); }}
          className="flex items-center gap-2 px-5 py-3 rounded-lg font-medium
                     bg-zinc-800 text-white border border-zinc-700 hover:border-zinc-600 
                     hover:bg-zinc-700 transition-all"
          data-testid="reset-btn"
        >
          <RotateCcw size={18} />
          Скинути
        </button>

        <button
          onClick={loadDemoRoute}
          className="flex items-center gap-2 px-5 py-3 rounded-lg font-medium
                     bg-zinc-800 text-white border border-zinc-700 hover:border-zinc-600 
                     hover:bg-zinc-700 transition-all"
          data-testid="new-route-btn"
        >
          <Plus size={18} />
          Новий маршрут
        </button>

        {saveEnabled && (
          <button
            onClick={handleSaveRoute}
            className="flex items-center gap-2 px-5 py-3 rounded-lg font-medium
                       bg-emerald-500/20 text-emerald-400 border border-emerald-500/30
                       hover:bg-emerald-500/30 transition-all animate-fade-in"
            data-testid="save-route-btn"
          >
            <Save size={18} />
            Зберегти
          </button>
        )}
      </GlassPanel>
    </div>
  );
};

// About Section Component
const AboutSection = () => (
  <div className="max-w-4xl mx-auto py-8 animate-fade-in" data-testid="about-section">
    <AnimatedCard className="overflow-hidden">
      {/* Header with gradient */}
      <div className="relative px-8 py-10 border-b border-zinc-800 overflow-hidden">
        <div className="absolute inset-0 bg-gradient-to-br from-cyan-500/10 via-transparent to-transparent" />
        <div className="relative flex items-center gap-5">
          <div className="p-4 rounded-2xl bg-cyan-500/10 border border-cyan-500/20">
            <Plane size={40} className="text-cyan-400" />
          </div>
          <div>
            <h1 className="font-heading font-black text-3xl text-white">Visual Homing System</h1>
            <p className="text-zinc-400 mt-1">Оптична навігація для ArduPilot</p>
          </div>
        </div>
      </div>

      <div className="p-8 space-y-8">
        <p className="text-zinc-400 text-lg leading-relaxed">
          Система візуальної навігації для мультикоптерних дронів на базі ArduPilot.
          Працює за принципом оптичної комп'ютерної мишки — записує візуальні орієнтири
          під час польоту та використовує їх для автономного повернення на точку зльоту.
        </p>

        {/* Features Grid */}
        <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
          {[
            { icon: '📍', title: 'Без GPS залежності', desc: 'Навігація по візуальних орієнтирах, стійкість до GPS спуфінгу' },
            { icon: '🧭', title: 'Без компаса', desc: 'Орієнтація визначається візуально, немає магнітних інтерференцій' },
            { icon: '🔄', title: 'Teach & Repeat', desc: 'Запис маршруту під час польоту, повернення по записаному шляху' },
            { icon: '🌡️', title: 'Термальна камера', desc: 'Підтримка Caddx Thermal 256 для нічного бачення' },
            { icon: '🔁', title: 'Smart RTL', desc: 'Гібридна навігація: IMU/Baro >50м + Optical Flow + Visual <50м' },
            { icon: '📡', title: 'Optical Flow', desc: 'MATEK 3901-L0X для точної навігації на малій висоті' },
          ].map((feature, idx) => (
            <AnimatedCard key={feature.title} delay={idx * 100} className="p-5 hover:-translate-y-1">
              <span className="text-3xl mb-3 block">{feature.icon}</span>
              <h3 className="font-heading font-bold text-white mb-2">{feature.title}</h3>
              <p className="text-sm text-zinc-500">{feature.desc}</p>
            </AnimatedCard>
          ))}
        </div>

        {/* Specs Table */}
        <div className="bg-zinc-900/50 rounded-xl border border-zinc-800 p-6">
          <h3 className="text-xs font-mono uppercase tracking-wider text-cyan-500 mb-4">Специфікації</h3>
          <div className="space-y-3">
            {[
              ['Комп\'ютер', 'Raspberry Pi Zero 2 W'],
              ['Камера', 'Caddx Thermal 256 / Pi Camera'],
              ['Політний контролер', 'Matek H743-Slim V3 (ArduCopter 4.5.7)'],
              ['Optical Flow', 'MATEK 3901-L0X (PMW3901 + VL53L0X)'],
              ['LiDAR', 'Benewake TF-Luna (0.2-8m)'],
              ['Протокол', 'MAVLink 2 / MSP V2 (UART)'],
              ['Мови', 'Python 3 / C++17'],
              ['Макс. політ', '5км на 200м (Smart RTL)'],
            ].map(([label, value]) => (
              <div key={label} className="flex justify-between items-center py-2 border-b border-zinc-800 last:border-0">
                <span className="text-zinc-500">{label}</span>
                <span className="font-mono font-medium text-white">{value}</span>
              </div>
            ))}
          </div>
        </div>
      </div>
    </AnimatedCard>
  </div>
);

// Sensor Status Card
const SensorCard = ({ title, icon: Icon, connected, children, testId }) => (
  <AnimatedCard className="p-5" data-testid={testId}>
    <div className="flex items-center justify-between mb-4">
      <div className="flex items-center gap-3">
        <div className={`p-2 rounded-lg border ${connected ? 'bg-emerald-500/10 border-emerald-500/20' : 'bg-zinc-800 border-zinc-700'}`}>
          <Icon size={20} className={connected ? 'text-emerald-400' : 'text-zinc-500'} />
        </div>
        <h3 className="font-heading font-bold text-white">{title}</h3>
      </div>
      <span className={`text-xs font-mono px-2 py-1 rounded-full ${connected ? 'bg-emerald-500/20 text-emerald-400' : 'bg-zinc-800 text-zinc-500'}`}>
        {connected ? 'ONLINE' : 'OFFLINE'}
      </span>
    </div>
    {children}
  </AnimatedCard>
);

// Metric display row
const MetricRow = ({ label, value, unit = '', color = 'text-white' }) => (
  <div className="flex justify-between items-center py-1.5 border-b border-zinc-800/50 last:border-0">
    <span className="text-zinc-500 text-sm">{label}</span>
    <span className={`font-mono font-medium ${color}`}>{value}{unit && <span className="text-zinc-600 ml-1">{unit}</span>}</span>
  </div>
);

// Smart RTL Phase indicator
const PhaseIndicator = ({ phase, active }) => {
  const phases = [
    { key: 'high_alt', label: 'HIGH ALT', icon: Navigation, desc: 'IMU/Baro' },
    { key: 'descent', label: 'DESCENT', icon: ArrowDown, desc: 'Зниження' },
    { key: 'low_alt', label: 'LOW ALT', icon: Radio, desc: 'Optical Flow' },
    { key: 'precision_land', label: 'LANDING', icon: Target, desc: 'Точна посадка' },
  ];

  return (
    <div className="grid grid-cols-4 gap-2 mt-4" data-testid="smart-rtl-phases">
      {phases.map((p, idx) => {
        const isActive = phase === p.key;
        const isPassed = active && phases.findIndex(x => x.key === phase) > idx;
        return (
          <div key={p.key} className={`text-center p-3 rounded-lg border transition-all duration-300
            ${isActive ? 'bg-cyan-500/20 border-cyan-500/40 scale-105' : isPassed ? 'bg-emerald-500/10 border-emerald-500/20' : 'bg-zinc-900/50 border-zinc-800'}`}>
            <p.icon size={18} className={`mx-auto mb-1 ${isActive ? 'text-cyan-400' : isPassed ? 'text-emerald-400' : 'text-zinc-600'}`} />
            <div className={`text-xs font-mono font-bold ${isActive ? 'text-cyan-400' : isPassed ? 'text-emerald-400' : 'text-zinc-600'}`}>{p.label}</div>
            <div className="text-[10px] text-zinc-600 mt-0.5">{p.desc}</div>
          </div>
        );
      })}
    </div>
  );
};

// Video Stream Component
const VideoStream = () => {
  const [streamStatus, setStreamStatus] = useState(null);

  useEffect(() => {
    axios.get(`${API}/stream/status`).then(r => setStreamStatus(r.data)).catch(() => {});
  }, []);

  const isHttps = window.location.protocol === 'https:';
  const streamUrl = streamStatus?.url || '';
  const isStreamHttp = streamUrl.startsWith('http://');
  const mixedContentBlocked = isHttps && isStreamHttp;

  return (
    <AnimatedCard className="p-5 overflow-hidden" data-testid="video-stream">
      <div className="flex items-center justify-between mb-4">
        <div className="flex items-center gap-3">
          <div className={`p-2 rounded-lg border ${streamStatus?.available && !mixedContentBlocked ? 'bg-emerald-500/10 border-emerald-500/20' : 'bg-zinc-800 border-zinc-700'}`}>
            <Video size={20} className={streamStatus?.available && !mixedContentBlocked ? 'text-emerald-400' : 'text-zinc-400'} />
          </div>
          <h3 className="font-heading font-bold text-white">Камера</h3>
        </div>
        <span className={`text-xs font-mono px-2 py-1 rounded-full ${
          mixedContentBlocked ? 'bg-amber-500/20 text-amber-400' :
          streamStatus?.available ? 'bg-emerald-500/20 text-emerald-400' : 'bg-zinc-800 text-zinc-500'}`}>
          {mixedContentBlocked ? 'HTTPS' : streamStatus?.available ? 'LIVE' : 'OFFLINE'}
        </span>
      </div>
      <div className="aspect-video bg-zinc-900 rounded-lg border border-zinc-800 flex items-center justify-center overflow-hidden">
        {streamStatus?.available && !mixedContentBlocked ? (
          <iframe 
            src={streamUrl}
            title="Camera Feed" 
            className="w-full h-full border-0"
            allow="autoplay"
            data-testid="video-feed"
          />
        ) : (
          <div className="text-center p-6">
            <Video size={48} className="mx-auto text-zinc-700 mb-3" />
            {mixedContentBlocked ? (
              <>
                <p className="text-amber-400 text-sm font-medium">HTTPS блокує HTTP стрім</p>
                <p className="text-zinc-500 text-xs mt-2">Стрім працює тільки через HTTP.</p>
                <p className="text-zinc-500 text-xs">Відкрийте додаток на Pi або через HTTP.</p>
                <a href={streamUrl} target="_blank" rel="noreferrer" 
                   className="inline-block mt-3 px-4 py-2 rounded-lg bg-cyan-500/20 text-cyan-400 text-xs font-mono hover:bg-cyan-500/30 transition-colors">
                  Відкрити стрім напряму
                </a>
              </>
            ) : (
              <>
                <p className="text-zinc-500 text-sm">Стрім вимкнено</p>
                <p className="text-zinc-600 text-xs mt-1 font-mono">{streamUrl}</p>
                <p className="text-zinc-600 text-xs mt-2">Налаштуйте URL в <span className="text-cyan-500">Налаштування</span></p>
              </>
            )}
          </div>
        )}
      </div>
    </AnimatedCard>
  );
};

// Settings Page Component
const SettingsPage = () => {
  const [settings, setSettings] = useState(null);
  const [saving, setSaving] = useState(false);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    axios.get(`${API}/settings`).then(r => { setSettings(r.data); setLoading(false); }).catch(() => setLoading(false));
  }, []);

  const handleSave = async () => {
    setSaving(true);
    try {
      await axios.post(`${API}/settings`, settings);
      toast.success("Налаштування збережено!");
    } catch (e) {
      toast.error("Помилка збереження");
    }
    setSaving(false);
  };

  const handleReset = async () => {
    try {
      const r = await axios.post(`${API}/settings/reset`);
      setSettings(r.data);
      toast.success("Налаштування скинуто!");
    } catch (e) {
      toast.error("Помилка скидання");
    }
  };

  const update = (key, val) => setSettings(prev => ({ ...prev, [key]: val }));

  if (loading || !settings) return (
    <div className="flex items-center justify-center h-64"><div className="w-8 h-8 border-2 border-cyan-500 border-t-transparent rounded-full animate-spin" /></div>
  );

  const SectionTitle = ({ children }) => (
    <h3 className="text-xs font-mono uppercase tracking-wider text-cyan-500 mb-3 mt-6 first:mt-0">{children}</h3>
  );

  const SettingRow = ({ label, children }) => (
    <div className="flex items-center justify-between py-3 border-b border-zinc-800/50">
      <span className="text-sm text-zinc-400">{label}</span>
      <div className="flex items-center gap-2">{children}</div>
    </div>
  );

  const Input = ({ value, onChange, type = "text", className = "w-40" }) => (
    <input
      type={type}
      value={value}
      onChange={(e) => onChange(type === "number" ? Number(e.target.value) : e.target.value)}
      className={`bg-zinc-900 border border-zinc-700 rounded-lg px-3 py-1.5 text-sm text-white font-mono 
                  focus:border-cyan-500 focus:outline-none ${className}`}
    />
  );

  const Toggle = ({ checked, onChange }) => (
    <Switch checked={checked} onCheckedChange={onChange} className="data-[state=checked]:bg-cyan-500" />
  );

  return (
    <div className="max-w-3xl mx-auto px-6 py-8 animate-fade-in" data-testid="settings-section">
      <div className="flex items-center justify-between mb-6">
        <div>
          <h2 className="font-heading font-bold text-2xl text-white">Налаштування</h2>
          <p className="text-zinc-500 text-sm mt-1">Конфігурація системи Visual Homing</p>
        </div>
        <div className="flex gap-2">
          <button onClick={handleReset} className="px-4 py-2 rounded-lg text-sm bg-zinc-800 text-zinc-400 border border-zinc-700 hover:text-white transition-colors" data-testid="settings-reset-btn">
            <RotateCcw size={14} className="inline mr-1" /> Скинути
          </button>
          <button onClick={handleSave} disabled={saving} className="px-4 py-2 rounded-lg text-sm bg-cyan-500 text-black font-bold hover:bg-cyan-400 transition-colors disabled:opacity-50" data-testid="settings-save-btn">
            <Save size={14} className="inline mr-1" /> {saving ? 'Збереження...' : 'Зберегти'}
          </button>
        </div>
      </div>

      <GlassPanel className="p-6">
        {/* Camera */}
        <SectionTitle>Камера</SectionTitle>
        <SettingRow label="Тип камери">
          <select value={settings.camera_type} onChange={(e) => update('camera_type', e.target.value)}
            className="bg-zinc-900 border border-zinc-700 rounded-lg px-3 py-1.5 text-sm text-white font-mono focus:border-cyan-500 focus:outline-none">
            <option value="usb_capture">USB Capture (EasyCap)</option>
            <option value="pi_camera">Pi Camera (CSI)</option>
          </select>
        </SettingRow>
        <SettingRow label="Пристрій"><Input value={settings.camera_device} onChange={(v) => update('camera_device', v)} /></SettingRow>
        <SettingRow label="Роздільність">
          <Input value={settings.camera_resolution_w} onChange={(v) => update('camera_resolution_w', v)} type="number" className="w-20" />
          <span className="text-zinc-600">x</span>
          <Input value={settings.camera_resolution_h} onChange={(v) => update('camera_resolution_h', v)} type="number" className="w-20" />
        </SettingRow>
        <SettingRow label="FPS"><Input value={settings.camera_fps} onChange={(v) => update('camera_fps', v)} type="number" className="w-20" /></SettingRow>
        <SettingRow label="Стрім"><Toggle checked={settings.stream_enabled} onChange={(v) => update('stream_enabled', v)} /></SettingRow>
        <SettingRow label="URL стріму"><Input value={settings.stream_url} onChange={(v) => update('stream_url', v)} className="w-64" /></SettingRow>

        {/* MAVLink */}
        <SectionTitle>MAVLink</SectionTitle>
        <SettingRow label="Порт"><Input value={settings.mavlink_port} onChange={(v) => update('mavlink_port', v)} /></SettingRow>
        <SettingRow label="Baud Rate"><Input value={settings.mavlink_baud} onChange={(v) => update('mavlink_baud', v)} type="number" className="w-28" /></SettingRow>

        {/* Optical Flow */}
        <SectionTitle>Optical Flow (MATEK 3901-L0X)</SectionTitle>
        <SettingRow label="Увімкнено"><Toggle checked={settings.flow_enabled} onChange={(v) => update('flow_enabled', v)} /></SettingRow>
        <SettingRow label="Порт"><Input value={settings.flow_port} onChange={(v) => update('flow_port', v)} /></SettingRow>
        <SettingRow label="Мін. якість"><Input value={settings.flow_min_quality} onChange={(v) => update('flow_min_quality', v)} type="number" className="w-20" /></SettingRow>

        {/* LiDAR */}
        <SectionTitle>LiDAR (TF-Luna)</SectionTitle>
        <SettingRow label="Увімкнено"><Toggle checked={settings.lidar_enabled} onChange={(v) => update('lidar_enabled', v)} /></SettingRow>
        <SettingRow label="Порт"><Input value={settings.lidar_port} onChange={(v) => update('lidar_port', v)} /></SettingRow>

        {/* Smart RTL */}
        <SectionTitle>Smart RTL</SectionTitle>
        <SettingRow label="Поріг висоти (м)"><Input value={settings.rtl_high_alt} onChange={(v) => update('rtl_high_alt', v)} type="number" className="w-24" /></SettingRow>
        <SettingRow label="Точна посадка (м)"><Input value={settings.rtl_precision_alt} onChange={(v) => update('rtl_precision_alt', v)} type="number" className="w-24" /></SettingRow>
        <SettingRow label="Початок зниження (%)"><Input value={(settings.rtl_descent_pct * 100).toFixed(0)} onChange={(v) => update('rtl_descent_pct', v / 100)} type="number" className="w-24" /></SettingRow>
        <SettingRow label="Швидкість зниження (м/с)"><Input value={settings.rtl_descent_rate} onChange={(v) => update('rtl_descent_rate', v)} type="number" className="w-24" /></SettingRow>
        <SettingRow label="Швидкість HIGH ALT (м/с)"><Input value={settings.rtl_high_speed} onChange={(v) => update('rtl_high_speed', v)} type="number" className="w-24" /></SettingRow>
        <SettingRow label="Швидкість LOW ALT (м/с)"><Input value={settings.rtl_low_speed} onChange={(v) => update('rtl_low_speed', v)} type="number" className="w-24" /></SettingRow>

        {/* System */}
        <SectionTitle>Система</SectionTitle>
        <SettingRow label="Автозапуск"><Toggle checked={settings.autostart} onChange={(v) => update('autostart', v)} /></SettingRow>
        <SettingRow label="Web порт"><Input value={settings.web_port} onChange={(v) => update('web_port', v)} type="number" className="w-24" /></SettingRow>
      </GlassPanel>
    </div>
  );
};

// Telemetry Dashboard Component
const TelemetryDashboard = () => {
  const [sensors, setSensors] = useState(null);
  const [rtlStatus, setRtlStatus] = useState(null);
  const [rtlConfig, setRtlConfig] = useState(null);
  const [loading, setLoading] = useState(true);

  const fetchData = useCallback(async () => {
    try {
      const [sensorsRes, rtlRes, configRes] = await Promise.all([
        axios.get(`${API}/sensors/status`),
        axios.get(`${API}/smart-rtl/status`),
        axios.get(`${API}/smart-rtl/config`)
      ]);
      setSensors(sensorsRes.data);
      setRtlStatus(rtlRes.data);
      setRtlConfig(configRes.data);
    } catch (e) {
      console.error("Failed to load telemetry:", e);
    }
    setLoading(false);
  }, []);

  useEffect(() => {
    fetchData();
    const interval = setInterval(fetchData, 2000);
    return () => clearInterval(interval);
  }, [fetchData]);

  if (loading) {
    return (
      <div className="flex items-center justify-center h-64">
        <div className="w-8 h-8 border-2 border-cyan-500 border-t-transparent rounded-full animate-spin" />
      </div>
    );
  }

  return (
    <div className="max-w-6xl mx-auto px-6 py-8 space-y-6 animate-fade-in" data-testid="telemetry-section">
      <div className="flex items-center justify-between mb-2">
        <div>
          <h2 className="font-heading font-bold text-2xl text-white">Телеметрія</h2>
          <p className="text-zinc-500 text-sm mt-1">Моніторинг сенсорів та навігації</p>
        </div>
        <button
          onClick={fetchData}
          className="p-2 rounded-lg hover:bg-white/5 text-zinc-400 hover:text-white transition-colors"
          data-testid="telemetry-refresh-btn"
        >
          <RotateCcw size={18} />
        </button>
      </div>

      {/* Video + Sensor Cards */}
      <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
        {/* Video Stream */}
        <VideoStream />

        {/* Optical Flow */}
        <SensorCard
          title="MATEK 3901-L0X"
          icon={sensors?.optical_flow_connected ? Wifi : WifiOff}
          connected={sensors?.optical_flow_connected}
          testId="sensor-optical-flow"
        >
          <div className="space-y-0.5">
            <MetricRow label="Optical Flow X" value={sensors?.flow_x?.toFixed(3) || '0.000'} unit="rad/s" />
            <MetricRow label="Optical Flow Y" value={sensors?.flow_y?.toFixed(3) || '0.000'} unit="rad/s" />
            <MetricRow
              label="Якість"
              value={sensors?.optical_flow_quality || 0}
              color={sensors?.optical_flow_quality > 50 ? 'text-emerald-400' : 'text-red-400'}
            />
          </div>
        </SensorCard>

        {/* LiDAR */}
        <SensorCard
          title="TF-Luna LiDAR"
          icon={sensors?.lidar_connected ? Wifi : WifiOff}
          connected={sensors?.lidar_connected}
          testId="sensor-lidar"
        >
          <div className="space-y-0.5">
            <MetricRow
              label="Відстань"
              value={sensors?.lidar_distance_m?.toFixed(2) || '0.00'}
              unit="m"
              color={sensors?.lidar_distance_m > 0 ? 'text-cyan-400' : 'text-zinc-500'}
            />
            <MetricRow label="Сигнал" value={sensors?.lidar_signal || 0} />
          </div>
        </SensorCard>
      </div>

      {/* Smart RTL Status */}
      <GlassPanel className="p-6" data-testid="smart-rtl-panel">
        <div className="flex items-center justify-between mb-4">
          <div className="flex items-center gap-3">
            <div className={`p-2 rounded-lg border ${rtlStatus?.active ? 'bg-cyan-500/10 border-cyan-500/20' : 'bg-zinc-800 border-zinc-700'}`}>
              <Navigation size={20} className={rtlStatus?.active ? 'text-cyan-400' : 'text-zinc-500'} />
            </div>
            <div>
              <h3 className="font-heading font-bold text-white">Smart RTL</h3>
              <p className="text-zinc-500 text-xs">Гібридна навігація повернення</p>
            </div>
          </div>
          <span className={`text-xs font-mono px-3 py-1.5 rounded-full border ${rtlStatus?.active ? 'bg-cyan-500/20 border-cyan-500/30 text-cyan-400' : 'bg-zinc-800 border-zinc-700 text-zinc-500'}`}>
            {rtlStatus?.active ? rtlStatus.phase?.toUpperCase() : 'IDLE'}
          </span>
        </div>

        {/* Metrics Grid */}
        <div className="grid grid-cols-2 md:grid-cols-4 gap-4 mb-4">
          {[
            { label: 'Висота', value: `${rtlStatus?.current_altitude?.toFixed(1) || 0}`, unit: 'm' },
            { label: 'До дому', value: `${rtlStatus?.home_distance?.toFixed(0) || 0}`, unit: 'm' },
            { label: 'Прогрес', value: `${((rtlStatus?.return_progress || 0) * 100).toFixed(0)}`, unit: '%' },
            { label: 'Джерело', value: rtlStatus?.nav_source || 'none', unit: '' },
          ].map(m => (
            <div key={m.label} className="bg-zinc-900/50 rounded-lg border border-zinc-800 p-3 text-center">
              <div className="text-xs text-zinc-500 mb-1">{m.label}</div>
              <div className="font-mono font-bold text-white text-lg">{m.value}<span className="text-zinc-600 text-xs ml-1">{m.unit}</span></div>
            </div>
          ))}
        </div>

        {/* Phase Progress */}
        <PhaseIndicator phase={rtlStatus?.phase} active={rtlStatus?.active} />

        {/* Config Summary */}
        {rtlConfig && (
          <div className="mt-4 pt-4 border-t border-zinc-800">
            <div className="text-xs font-mono text-zinc-600 mb-2">КОНФІГУРАЦІЯ</div>
            <div className="grid grid-cols-3 gap-3 text-center">
              <div className="bg-zinc-950/50 rounded-lg p-2">
                <div className="text-xs text-zinc-500">Поріг висоти</div>
                <div className="font-mono text-sm text-white">{rtlConfig.high_alt_threshold}м</div>
              </div>
              <div className="bg-zinc-950/50 rounded-lg p-2">
                <div className="text-xs text-zinc-500">Початок зниження</div>
                <div className="font-mono text-sm text-white">{rtlConfig.descent_start_pct * 100}%</div>
              </div>
              <div className="bg-zinc-950/50 rounded-lg p-2">
                <div className="text-xs text-zinc-500">Точна посадка</div>
                <div className="font-mono text-sm text-white">{rtlConfig.precision_land_alt}м</div>
              </div>
            </div>
          </div>
        )}
      </GlassPanel>
    </div>
  );
};

// Main App Component
function App() {
  const [activeTab, setActiveTab] = useState('map');
  const [docs, setDocs] = useState([]);
  const [selectedDoc, setSelectedDoc] = useState(null);
  const [docContent, setDocContent] = useState(null);
  const [firmware, setFirmware] = useState({ python: [], cpp: [], scripts: [], config: [] });
  const [selectedFile, setSelectedFile] = useState(null);
  const [fileContent, setFileContent] = useState(null);
  const [loading, setLoading] = useState(false);
  const [savedRoutes, setSavedRoutes] = useState([]);
  const [saveEnabled, setSaveEnabled] = useState(false);
  const [routesLoading, setRoutesLoading] = useState(false);
  const [selectedRoute, setSelectedRoute] = useState(null);

  // Load documentation list
  useEffect(() => {
    const loadDocs = async () => {
      try {
        const response = await axios.get(`${API}/docs/list`);
        setDocs(response.data);
      } catch (e) {
        console.error("Failed to load docs:", e);
      }
    };
    loadDocs();
  }, []);

  // Load firmware structure
  useEffect(() => {
    const loadFirmware = async () => {
      try {
        const response = await axios.get(`${API}/firmware/structure`);
        setFirmware(response.data);
      } catch (e) {
        console.error("Failed to load firmware structure:", e);
      }
    };
    loadFirmware();
  }, []);

  // Load saved routes
  useEffect(() => {
    if (activeTab === 'history') {
      loadSavedRoutes();
    }
  }, [activeTab]);

  const loadSavedRoutes = async () => {
    setRoutesLoading(true);
    try {
      const response = await axios.get(`${API}/routes`);
      setSavedRoutes(response.data);
    } catch (e) {
      console.error("Failed to load routes:", e);
    }
    setRoutesLoading(false);
  };

  const loadDoc = useCallback(async (filename) => {
    setLoading(true);
    try {
      const response = await axios.get(`${API}/docs/${filename}`);
      setDocContent(response.data);
    } catch (e) {
      console.error("Failed to load doc:", e);
    }
    setLoading(false);
  }, []);

  const loadFile = useCallback(async (filepath) => {
    setLoading(true);
    try {
      const response = await axios.get(`${API}/firmware/file/${filepath}`);
      setFileContent(response.data.content);
    } catch (e) {
      console.error("Failed to load file:", e);
    }
    setLoading(false);
  }, []);

  const handleDocSelect = (doc) => {
    setSelectedDoc(doc.name);
    loadDoc(doc.name);
  };

  const handleFileSelect = (path) => {
    setSelectedFile(path);
    loadFile(path);
  };

  const handleSaveRoute = async (route) => {
    const savedRoute = {
      ...route,
      id: `route_${Date.now()}`,
      name: `Маршрут ${new Date().toLocaleString('uk-UA')}`,
      created_at: new Date().toISOString()
    };
    await axios.post(`${API}/routes`, savedRoute);
    toast.success("Маршрут збережено!");
    loadSavedRoutes();
  };

  const handleDeleteRoute = async (routeId) => {
    try {
      await axios.delete(`${API}/routes/${routeId}`);
      toast.success("Маршрут видалено");
      loadSavedRoutes();
    } catch (e) {
      toast.error("Помилка видалення");
    }
  };

  const allFirmwareFiles = [
    ...firmware.python,
    ...firmware.cpp,
    ...firmware.scripts,
    ...firmware.config
  ];

  return (
    <div className="min-h-screen bg-zinc-950 text-white font-sans" data-testid="app-container">
      <Toaster position="top-right" theme="dark" />
      
      {/* Header */}
      <header className="sticky top-0 z-50 border-b border-zinc-800 bg-zinc-950/80 backdrop-blur-xl" data-testid="header">
        <div className="max-w-[1800px] mx-auto px-4 md:px-6 h-14 md:h-16 flex items-center justify-between">
          {/* Logo */}
          <div className="flex items-center gap-2 md:gap-3 shrink-0">
            <div className="p-1.5 md:p-2 rounded-lg bg-cyan-500/10 border border-cyan-500/20">
              <Plane size={18} className="text-cyan-400" />
            </div>
            <h1 className="font-heading font-bold text-base md:text-lg text-white hidden sm:block">Visual Homing</h1>
            <span className="px-1.5 py-0.5 rounded-full text-[10px] font-mono bg-zinc-800 text-zinc-400 hidden sm:block">v2.2</span>
          </div>

          {/* Navigation - scrollable on mobile */}
          <nav className="flex items-center gap-1 md:gap-2 overflow-x-auto scrollbar-hide ml-2 md:ml-4" data-testid="nav-tabs">
            <TabButton 
              active={activeTab === 'map'} 
              icon={Map} 
              label="3D Карта" 
              onClick={() => setActiveTab('map')}
              testId="tab-map"
            />
            <TabButton 
              active={activeTab === 'history'} 
              icon={History} 
              label="Історія" 
              onClick={() => setActiveTab('history')}
              testId="tab-history"
            />
            <TabButton 
              active={activeTab === 'telemetry'} 
              icon={Activity} 
              label="Телеметрія" 
              onClick={() => setActiveTab('telemetry')}
              testId="tab-telemetry"
            />
            <TabButton 
              active={activeTab === 'docs'} 
              icon={FileText} 
              label="Документація" 
              onClick={() => setActiveTab('docs')}
              testId="tab-docs"
            />
            <TabButton 
              active={activeTab === 'firmware'} 
              icon={Code} 
              label="Прошивка" 
              onClick={() => setActiveTab('firmware')}
              testId="tab-firmware"
            />
            <TabButton 
              active={activeTab === 'about'} 
              icon={Info} 
              label="Про проект" 
              onClick={() => setActiveTab('about')}
              testId="tab-about"
            />
            <TabButton 
              active={activeTab === 'settings'} 
              icon={Settings} 
              label="Налаштування" 
              onClick={() => setActiveTab('settings')}
              testId="tab-settings"
            />
          </nav>
        </div>
      </header>

      {/* Main Content */}
      <main>
        {activeTab === 'map' && (
          <MapPanel
            onSaveRoute={handleSaveRoute}
            saveEnabled={saveEnabled}
            setSaveEnabled={setSaveEnabled}
            selectedRoute={selectedRoute}
          />
        )}

        {activeTab === 'history' && (
          <div className="max-w-4xl mx-auto px-6 py-8" data-testid="history-section">
            <div className="flex items-center justify-between mb-6">
              <div>
                <h2 className="font-heading font-bold text-2xl text-white">Історія маршрутів</h2>
                <p className="text-zinc-500 text-sm mt-1">Збережені записи польотів</p>
              </div>
              <button 
                onClick={loadSavedRoutes}
                className="p-2 rounded-lg hover:bg-white/5 text-zinc-400 hover:text-white transition-colors"
              >
                <RotateCcw size={18} />
              </button>
            </div>
            <RouteHistory 
              routes={savedRoutes} 
              onSelect={(route) => { setSelectedRoute(route); setActiveTab('map'); }}
              onDelete={handleDeleteRoute}
              loading={routesLoading}
            />
          </div>
        )}

        {activeTab === 'telemetry' && (
          <TelemetryDashboard />
        )}

        {activeTab === 'docs' && (
          <div className="max-w-7xl mx-auto px-6 py-8 grid grid-cols-1 lg:grid-cols-4 gap-6" data-testid="docs-section">
            {/* Sidebar */}
            <aside className="lg:col-span-1">
              <GlassPanel className="p-4 sticky top-24">
                <h2 className="text-xs font-mono uppercase tracking-wider text-cyan-500 mb-4 px-2">
                  Документи
                </h2>
                <div className="space-y-1">
                  {docs.map((doc, idx) => (
                    <button
                      key={doc.name}
                      onClick={() => handleDocSelect(doc)}
                      data-testid={`doc-item-${doc.name}`}
                      className={`w-full text-left px-3 py-2.5 rounded-lg text-sm transition-all duration-200
                                 animate-slide-up flex items-center gap-2
                                 ${selectedDoc === doc.name 
                                   ? 'bg-cyan-950/50 border border-cyan-500/30 text-cyan-400' 
                                   : 'text-zinc-400 hover:text-white hover:bg-white/5'}`}
                      style={{ animationDelay: `${idx * 50}ms` }}
                    >
                      <FileText size={14} />
                      <span className="truncate">{doc.title}</span>
                    </button>
                  ))}
                </div>
              </GlassPanel>
            </aside>

            {/* Content */}
            <section className="lg:col-span-3">
              <GlassPanel className="min-h-[600px] overflow-hidden">
                {loading ? (
                  <div className="flex items-center justify-center h-64">
                    <div className="w-8 h-8 border-2 border-cyan-500 border-t-transparent rounded-full animate-spin" />
                  </div>
                ) : (
                  <DocViewer doc={docContent} />
                )}
              </GlassPanel>
            </section>
          </div>
        )}

        {activeTab === 'firmware' && (
          <div className="max-w-7xl mx-auto px-6 py-8 grid grid-cols-1 lg:grid-cols-4 gap-6" data-testid="firmware-section">
            {/* Sidebar */}
            <aside className="lg:col-span-1">
              <GlassPanel className="p-4 sticky top-24 max-h-[calc(100vh-120px)] overflow-y-auto">
                <h2 className="text-xs font-mono uppercase tracking-wider text-cyan-500 mb-4 px-2">
                  Файли прошивки
                </h2>
                <FileTree
                  files={allFirmwareFiles}
                  onSelect={handleFileSelect}
                  selectedPath={selectedFile}
                />
              </GlassPanel>
            </aside>

            {/* Content */}
            <section className="lg:col-span-3">
              <GlassPanel className="min-h-[600px] overflow-hidden">
                {loading ? (
                  <div className="flex items-center justify-center h-64">
                    <div className="w-8 h-8 border-2 border-cyan-500 border-t-transparent rounded-full animate-spin" />
                  </div>
                ) : (
                  <CodeViewer code={fileContent} path={selectedFile} />
                )}
              </GlassPanel>
            </section>
          </div>
        )}

        {activeTab === 'about' && (
          <div className="px-6">
            <AboutSection />
          </div>
        )}

        {activeTab === 'settings' && (
          <SettingsPage />
        )}
      </main>

      {/* Footer */}
      <footer className="border-t border-zinc-800 py-6 mt-8" data-testid="footer">
        <p className="text-center text-zinc-600 text-sm">
          Visual Homing System • ArduPilot + Raspberry Pi Zero 2 W • MIT License
        </p>
      </footer>
    </div>
  );
}

export default App;
