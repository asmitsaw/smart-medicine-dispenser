import requests

# We will fill these in once you provide your Token and Chat ID!
TELEGRAM_BOT_TOKEN = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
TELEGRAM_CHAT_ID = "XXXXXXXXXXXXXXXXXXXX"

def send_telegram_message(message):
    """Sends a text message to the configured Telegram chat."""
    if not TELEGRAM_BOT_TOKEN or not TELEGRAM_CHAT_ID:
        print("[Telegram] Missing Bot Token or Chat ID!")
        return False
        
    url = f"https://api.telegram.org/bot{TELEGRAM_BOT_TOKEN}/sendMessage"
    payload = {
        "chat_id": TELEGRAM_CHAT_ID,
        "text": message,
        "parse_mode": "Markdown"
    }
    
    try:
        response = requests.post(url, json=payload, timeout=5)
        response.raise_for_status()
        print(f"[Telegram] Message sent: {message}")
        return True
    except Exception as e:
        print(f"[Telegram] Failed to send message: {e}")
        return False

def send_telegram_photo(photo_path, caption=""):
    """Sends a photo with an optional caption to the configured Telegram chat."""
    if not TELEGRAM_BOT_TOKEN or not TELEGRAM_CHAT_ID:
        print("[Telegram] Missing Bot Token or Chat ID!")
        return False
        
    url = f"https://api.telegram.org/bot{TELEGRAM_BOT_TOKEN}/sendPhoto"
    
    try:
        with open(photo_path, "rb") as photo_file:
            payload = {"chat_id": TELEGRAM_CHAT_ID, "caption": caption}
            files = {"photo": photo_file}
            response = requests.post(url, data=payload, files=files, timeout=10)
            response.raise_for_status()
            print(f"[Telegram] Photo sent successfully!")
            return True
    except Exception as e:
        print(f"[Telegram] Failed to send photo: {e}")
        return False
