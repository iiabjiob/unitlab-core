from starlette.middleware.base import BaseHTTPMiddleware
from starlette.requests import Request
from starlette.responses import JSONResponse
import traceback
from app.core.logger import logger

class ExceptionMiddleware(BaseHTTPMiddleware):
    """ Global exception handler middleware """

    async def dispatch(self, request: Request, call_next):
        try:
            return await call_next(request)
        except Exception as e:
            error_id = id(e)
            logger.error(f"❌ Unhandled Exception [{error_id}]: {str(e)}")
            logger.debug(f"🔍 Traceback [{error_id}]: {traceback.format_exc()}")

            return JSONResponse(
                content={"error": "Internal Server Error", "error_id": error_id},
                status_code=500
            )
