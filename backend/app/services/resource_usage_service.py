import psutil

class ResourceUsageService:
    """ Получает данные о загрузке CPU, памяти и диска """

    @staticmethod
    def get_cpu_usage():
        return f"{psutil.cpu_percent(interval=1)}%"

    @staticmethod
    def get_ram_usage():
        ram = psutil.virtual_memory()
        return {
            "total": round(ram.total / (1024 ** 3), 2),
            "used": round(ram.used / (1024 ** 3), 2),
            "available": round(ram.available / (1024 ** 3), 2),
        }

    @staticmethod
    def get_disk_usage():
        disk = psutil.disk_usage("/")
        return {
            "total": round(disk.total / (1024 ** 3), 2),
            "used": round(disk.used / (1024 ** 3), 2),
            "free": round(disk.free / (1024 ** 3), 2),
        }