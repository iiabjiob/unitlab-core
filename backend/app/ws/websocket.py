from fastapi import APIRouter, WebSocket, WebSocketDisconnect
from app.services.websocket_manager import ws_manager
from app.core.logger import logger

router = APIRouter()

@router.websocket("/ws/{topic}")
async def websocket_endpoint(websocket: WebSocket, topic: str):
    """ WebSocket API с логированием соединений """
    try:
        logger.info(f"📡 Attempting WebSocket connection on topic: {topic}")
        # Принимаем соединение
        await websocket.accept()
        # Добавляем соединение в менеджер
        await ws_manager.connect(websocket, topic)
        logger.info(f"✅ WebSocket connection accepted on topic: {topic}")
        
        # Основной цикл для обработки сообщений
        while True:
            try:
                # Ожидаем данные от клиента
                data = await websocket.receive_text()
                logger.debug(f"📩 Received message on {topic}: {data}")
                # Обработка данных (пример: отправка обратно клиенту)
                response = {"topic": topic, "message": data}
                await ws_manager.send_data(topic, response)

            except WebSocketDisconnect:
                # Обработка отключения клиента
                logger.warning(f"❌ WebSocket disconnected on topic: {topic}")
                break

            except Exception as e:
                # Обработка других ошибок
                logger.error(f"⚠️ WebSocket error on topic {topic}: {e}")
                break

    except Exception as e:
        # Обработка ошибок при подключении
        logger.error(f"⚠️ WebSocket error on topic {topic}: {e}")
        return

    finally:
        # Удаляем соединение из менеджера при завершении
        await ws_manager.disconnect(websocket, topic)
        logger.info(f"➖ WebSocket removed from manager on topic: {topic}")