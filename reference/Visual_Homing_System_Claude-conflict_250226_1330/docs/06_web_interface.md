# Веб-інтерфейс моніторингу

## Опис

Веб-інтерфейс для моніторингу системи Visual Homing в реальному часі.

## Функції

1. **Відео з камери** - пряма трансляція
2. **Статус системи**:
   - Режим (запис/повернення/очікування)
   - Кількість keyframes
   - GPS статус
   - Барометр (висота)
3. **Керування**:
   - Почати запис
   - Почати повернення
   - Стоп
4. **Візуалізація**:
   - Візуальні фічі на кадрі
   - Прогрес маршруту
   - MAVLink статистика

## Доступ

```
http://visual-homing.local:5000
```

Або по IP:
```
http://192.168.1.xxx:5000
```

## Технології

- **Backend**: Flask + Flask-SocketIO
- **Frontend**: React (Vite)
- **Streaming**: WebSocket + MJPEG

## Запуск

```bash
# Автоматично з Visual Homing
cd ~/visual_homing
source venv/bin/activate
python main.py --web

# Або окремо
python -m web.server
```
