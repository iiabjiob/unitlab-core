# app/api/health_router.py
from fastapi import APIRouter

router = APIRouter()

@router.get("/health", tags=["Health"])
async def health():
    return {"status": "ok"}
