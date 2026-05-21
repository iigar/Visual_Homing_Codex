import { useRef, useMemo, useState } from 'react';
import { Canvas, useFrame, useThree } from '@react-three/fiber';
import { OrbitControls } from '@react-three/drei';
import * as THREE from 'three';

// Ground grid component
const Ground = () => {
  return (
    <group>
      <gridHelper args={[100, 20, '#2a2a35', '#1a1a25']} />
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, -0.01, 0]}>
        <planeGeometry args={[100, 100]} />
        <meshBasicMaterial color="#0a0a0f" transparent opacity={0.8} />
      </mesh>
    </group>
  );
};

// Drone model component using basic Three.js primitives
const Drone = ({ position, rotation, mode }) => {
  const droneRef = useRef();
  
  const modeColor = useMemo(() => {
    const colors = {
      'IDLE': '#888888',
      'RECORDING': '#ff4444',
      'RETURNING': '#44ff44',
      'FLYING': '#4ecdc4'
    };
    return colors[mode] || '#4ecdc4';
  }, [mode]);
  
  // Animate drone hover
  useFrame((state) => {
    if (droneRef.current) {
      droneRef.current.position.y = position[1] + Math.sin(state.clock.elapsedTime * 2) * 0.1;
    }
  });
  
  return (
    <group 
      ref={droneRef} 
      position={position} 
      rotation={[0, rotation || 0, 0]}
    >
      {/* Drone body */}
      <mesh>
        <boxGeometry args={[0.8, 0.2, 0.8]} />
        <meshStandardMaterial color={modeColor} metalness={0.8} roughness={0.2} />
      </mesh>
      
      {/* Arms and motors */}
      {[[0.5, 0, 0.5], [-0.5, 0, 0.5], [0.5, 0, -0.5], [-0.5, 0, -0.5]].map((pos, i) => (
        <group key={i} position={pos}>
          <mesh>
            <cylinderGeometry args={[0.05, 0.05, 0.6]} />
            <meshStandardMaterial color="#333" />
          </mesh>
          <mesh position={[0, 0.35, 0]}>
            <cylinderGeometry args={[0.15, 0.15, 0.1]} />
            <meshStandardMaterial color="#222" />
          </mesh>
        </group>
      ))}
      
      {/* Camera indicator */}
      <mesh position={[0.4, -0.15, 0]} rotation={[Math.PI / 4, 0, 0]}>
        <coneGeometry args={[0.1, 0.15, 4]} />
        <meshStandardMaterial color="#4ecdc4" emissive="#4ecdc4" emissiveIntensity={0.5} />
      </mesh>
      
      {/* Status light */}
      <pointLight position={[0, 0.3, 0]} color={modeColor} intensity={2} distance={3} />
    </group>
  );
};

// Flight path using native Three.js line
const FlightPath = ({ points, color = '#4ecdc4' }) => {
  const lineRef = useRef();
  
  const geometry = useMemo(() => {
    if (!points || points.length < 2) return null;
    
    const linePoints = points.map(p => new THREE.Vector3(p.x, p.z, -p.y));
    const geo = new THREE.BufferGeometry().setFromPoints(linePoints);
    return geo;
  }, [points]);
  
  if (!geometry) return null;
  
  return (
    <line ref={lineRef}>
      <primitive object={geometry} attach="geometry" />
      <lineBasicMaterial color={color} linewidth={2} transparent opacity={0.8} />
    </line>
  );
};

// Keyframe markers using spheres
const KeyframeMarkers = ({ keyframes }) => {
  if (!keyframes || keyframes.length === 0) return null;
  
  return (
    <group>
      {keyframes.map((kf, i) => (
        <group key={i} position={[kf.x, kf.z, -kf.y]}>
          {/* Marker sphere */}
          <mesh>
            <sphereGeometry args={[0.5, 16, 16]} />
            <meshStandardMaterial 
              color="#ff6b6b" 
              emissive="#ff6b6b" 
              emissiveIntensity={0.3}
              transparent
              opacity={0.8}
            />
          </mesh>
          {/* Vertical line to ground */}
          <mesh>
            <cylinderGeometry args={[0.03, 0.03, kf.z, 8]} />
            <meshBasicMaterial color="#ff6b6b" transparent opacity={0.3} />
          </mesh>
        </group>
      ))}
    </group>
  );
};

// Home marker
const HomeMarker = () => {
  return (
    <group position={[0, 0, 0]}>
      <mesh position={[0, 0.75, 0]}>
        <coneGeometry args={[0.8, 1.5, 4]} />
        <meshStandardMaterial color="#ffd700" emissive="#ffd700" emissiveIntensity={0.3} />
      </mesh>
    </group>
  );
};

// Camera follow controller
const CameraController = ({ target, followDrone }) => {
  const { camera } = useThree();
  
  useFrame(() => {
    if (followDrone && target) {
      const targetPos = new THREE.Vector3(target[0], target[1] + 15, target[2] + 25);
      camera.position.lerp(targetPos, 0.02);
      camera.lookAt(target[0], target[1], target[2]);
    }
  });
  
  return null;
};

// Main 3D Map component
const RouteMap3D = ({ route, dronePosition, mode = 'IDLE', followDrone = false }) => {
  const position = dronePosition ? [dronePosition.x, dronePosition.z, -dronePosition.y] : [0, 5, 0];
  
  return (
    <Canvas camera={{ position: [50, 40, 50], fov: 60 }}>
      {/* Lighting */}
      <ambientLight intensity={0.4} />
      <directionalLight position={[50, 50, 50]} intensity={1} />
      <pointLight position={[0, 30, 0]} intensity={0.5} color="#4ecdc4" />
      
      {/* Controls */}
      <OrbitControls 
        enablePan={true}
        enableZoom={true}
        enableRotate={true}
        maxPolarAngle={Math.PI / 2.1}
        minDistance={10}
        maxDistance={200}
      />
      
      {/* Camera follow */}
      <CameraController target={position} followDrone={followDrone} />
      
      {/* Ground */}
      <Ground />
      
      {/* Home marker */}
      <HomeMarker />
      
      {/* Flight path */}
      {route && route.points && (
        <FlightPath points={route.points} color="#4ecdc4" />
      )}
      
      {/* Keyframes */}
      {route && route.keyframes && (
        <KeyframeMarkers keyframes={route.keyframes} />
      )}
      
      {/* Drone */}
      <Drone position={position} rotation={dronePosition?.yaw || 0} mode={mode} />
      
      {/* Sky */}
      <mesh>
        <sphereGeometry args={[300, 32, 32]} />
        <meshBasicMaterial color="#0a0a1a" side={THREE.BackSide} />
      </mesh>
    </Canvas>
  );
};

export default RouteMap3D;
