import cv2
import numpy as np
import base64
import json
import asyncio
import websockets

async def test():
    # create a mock 800x600 image
    img = np.zeros((600, 800, 3), dtype=np.uint8)
    _, buffer = cv2.imencode('.jpg', img)
    b64 = base64.b64encode(buffer).decode('utf-8')
    data = "data:image/jpeg;base64," + b64
    
    try:
        async with websockets.connect("ws://localhost:8081") as ws:
            await ws.send(json.dumps({"type": "frame", "image": data}))
            res = await ws.recv()
            print(res)
    except Exception as e:
        print("Error:", e)

asyncio.run(test())
