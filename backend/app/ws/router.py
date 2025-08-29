from fastapi import APIRouter, WebSocket, WebSocketDisconnect
from pydantic import ValidationError, TypeAdapter
from app.ws.manager import WebSocketManager
from app.schemas.ws.messages import WSMessage
from app.ws.actions.base import ACTION_HANDLERS
from app.core.logger import get_logger

logger = get_logger("ws")

router = APIRouter(prefix="/ws", tags=["Websocket"])
adapter = TypeAdapter(WSMessage)


@router.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    ws_manager = WebSocketManager.get_instance()
    
    await ws_manager.connect(websocket)
    logger.info("✅ WS client connected")

    try:
        while True:
            try:
                raw = await websocket.receive_json()
                message = adapter.validate_python(raw)
            except WebSocketDisconnect:
                logger.info("✅ Client disconnected cleanly")
                break
            except ValidationError as e:
                logger.warning(f"⚠️ Invalid WS message: {e}")
                continue
            except Exception as e:
                logger.exception(f"💥 Error while receiving or validating message: {e}")
                break

            logger.info(f"📨 IN ← {message.action} | {message.model_dump_json()}")
            
            handler = ACTION_HANDLERS.get(message.action)
            if handler:
                try:
                    await handler(websocket, message)
                    
                except Exception as e:
                    logger.exception(f"💥 Handler error for action '{message.action}': {e}")
            else:
                logger.warning(f"⚠️ Unknown action: {message.action}")

    except Exception as e:
        logger.exception(f"💥 Unexpected WS error: {e}")
    finally:
        ws_manager.disconnect(websocket)
