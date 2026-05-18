import requests
import time
import sys

TOKEN = "8497843059:AAEWKBe166tHBlUWZC0XVNB3cDuODGSCB7A"

print("Waiting for you to send a message to the bot...")
for _ in range(30):
    try:
        url = f"https://api.telegram.org/bot{TOKEN}/getUpdates"
        r = requests.get(url).json()
        if r.get("ok") and len(r["result"]) > 0:
            chat_id = r["result"][0]["message"]["chat"]["id"]
            print(f"\nSUCCESS! Your Chat ID is: {chat_id}")
            print("Please tell Antigravity the Chat ID so it can save it.")
            sys.exit(0)
    except Exception as e:
        pass
    time.sleep(2)

print("\nDid not receive a message in time. Please try running this script again after messaging the bot.")
