import sys
import time

sys.path.append("build-debug/py")

import miniexchange_client # type: ignore



if __name__ == "__main__":
    hmac_key = [0x11] * 32
    api_key = [0x22] * 16

    client = miniexchange_client.MiniExchangeClient(
        hmac_key=hmac_key,
        api_key=api_key
    )
    if client.connect():
        client.send_hello()
        time.sleep(1)
        msgs = client.poll_messages()

        for msg in msgs:
            print("got", msg)
        else:
            print("got nothing")

        client.send_order(100, 50.0, True, True)
        time.sleep(1)
        msgs = client.poll_messages()

        for msg in msgs:
            print("got", msg)
        else:
            print("got nothing")
    
        client.stop()
    else:
        print("failed to connect")
