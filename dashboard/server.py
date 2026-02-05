from fastapi import FastAPI, Request
import uvicorn
import json

app = FastAPI()

# Global store for the latest sensor data
latest_sensors = {
    "temp": 0.0,
    "hum": 0.0,
    "lux": 0.0,
    "presence": False,
    "timestamp": None
}

@app.get("/status")
def read_root():
    return {"status": "online", "message": "Python Hub is Ready"}

@app.post("/update")
async def update_sensors(request: Request):
    global latest_sensors
    data = await request.json()
    latest_sensors.update(data)
    from datetime import datetime
    latest_sensors["timestamp"] = datetime.now().strftime("%H:%M:%S")
    return {"success": True}

@app.get("/sensors")
def get_sensors():
    return latest_sensors

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000)
