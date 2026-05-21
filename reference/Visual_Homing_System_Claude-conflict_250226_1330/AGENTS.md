# AGENTS.md — Налаштування AI агентів

> Конфігурація для AI агентів, що працюють над проектом Visual Homing.

---

## Основний агент

**Роль:** Full-stack розробник для системи Visual Homing.
**Мова:** Українська — завжди відповідай українською.

**Читати перед початком роботи:**
1. `CLAUDE.md` — технічний контекст
2. `memory/PRD.md` — вимоги до продукту
3. `docs/README.md` — індекс документації

---

## Ключові обмеження

```yaml
порти:
  backend: 8001
  frontend: 3000
  pi_web: 5000

пакетні_менеджери:
  frontend: yarn
  backend: pip

тести:
  backend_url: http://localhost:8001   # НЕ Emergent cloud URL
  frontend_url: http://localhost:3000  # НЕ Emergent cloud URL

системні_папки:
  не_видаляти:
    - .git
    - .emergent
```

---

## Testing Agent

### Коли запускати
- Після нової функції
- Після виправлення P1 багу
- Перед мержем

### Команди
```bash
# Backend tests
cd backend && pytest tests/ -v

# E2E tests
cd tests && npx playwright test
```

---

## Архітектурні рішення

- **Teach & Repeat** — не real-time VO, а запис маршруту і повернення по ньому
- **Python primary** — Python для прототипу, C++ для продуктивності
- **MongoDB** — JSON-документи для keyframe-структури маршрутів
- **Three.js без @react-three/drei** — менша залежність, прямий контроль WebGL
- **Smart RTL** — гібридна навігація: IMU/Baro (>50m) + Optical Flow/Visual (<50m)
