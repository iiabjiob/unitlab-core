from fastapi import APIRouter

router = APIRouter(prefix="/api", tags=["Status"])

@router.get("/status")
async def health_check():
    """ Returns server health status """
    return {"status": "ok"}
