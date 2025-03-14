from fastapi import APIRouter

router = APIRouter(prefix="/api", tags=["Health"])

@router.get("/health")
async def health_check():
    """ Returns server health status """
    return {"status": "ok"}
