import asyncio
import websockets

async def hello():
    try:
        async with websockets.connect("ws://localhost:8081") as ws:
            print("Connected!")
            await ws.send('{"type": "get_ports"}')
            print("Sent get_ports")
            res = await ws.recv()
            print("Received:", res)
    except Exception as e:
        print("Error:", e)

asyncio.run(hello())
