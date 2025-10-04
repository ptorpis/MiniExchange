import sys
import time

sys.path.append("build-debug/py")

import miniexchange_client # type: ignore



if __name__ == "__main__":
    hmac_key = [0x11] * 32   # list of 32 integers
    api_key = [0x22] * 16    # list of 16 integers

    client = miniexchange_client.MiniExchangeClient(
        hmac_key=hmac_key,
        api_key=api_key
    )
    client.connect()

    client.send_hello()
    time.sleep(0.1)
    client.send_order(100, 50.0, True, True)
    time.sleep(0.1)


    client.disconnect()