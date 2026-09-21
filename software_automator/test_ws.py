import asyncio
import websockets

async def handler(websocket):
    print("Handler called with 1 arg!")
    await websocket.send("hello")

async def handler2(websocket, path):
    print("Handler called with 2 args!")
    await websocket.send("hello")

async def main():
    try:
        async with websockets.serve(handler, "localhost", 8082):
            await asyncio.sleep(0.5)
    except Exception as e:
        print("Error with 1 arg:", e)

    try:
        async with websockets.serve(handler2, "localhost", 8083):
            await asyncio.sleep(0.5)
    except Exception as e:
        print("Error with 2 args:", e)

asyncio.run(main())
