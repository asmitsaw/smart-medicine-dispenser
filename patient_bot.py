import time
import requests
import google.generativeai as genai

# ==========================================
# CONFIGURATION
# ==========================================
TELEGRAM_TOKEN = "8690875925:AAFYcEI-LgaUP4k2E2eLsS6E7rDWq__IRhg"
GEMINI_API_KEY = "AIzaSyD-YebruxxYEYqADi6Dxn8bjSkrCjAAfEw"  # Get this from https://aistudio.google.com/

# ==========================================
# GEMINI INITIALIZATION
# ==========================================
# We configure a helpful system prompt to ensure it behaves like a safe medical assistant
SYSTEM_PROMPT = """
You are 'Senior Medicine Assistant', a friendly, compassionate, and helpful medical AI bot.
Your primary role is to assist patients by answering basic questions about diseases, diagnostics, 
medicine names, side effects, and correct usage.

Guidelines:
0. ALWAYS communicate entirely in English unless the patient explicitly speaks to you in a different language.
1. Be very polite, empathetic, and reassuring to the patient.
2. Provide clear, simple, and easy-to-understand explanations for medical queries.
3. If they ask about a medicine, explain what it's for, potential common side effects, and best practices (e.g., take with food).
4. ALWAYS include a small disclaimer at the end of diagnostic or serious advice reminding them to "Please consult a real human doctor or a pharmacist for professional medical advice."
5. Format your messages cleanly using bullet points or bold text to make it easy to read on a phone.
"""

def setup_gemini():
    genai.configure(api_key=GEMINI_API_KEY)
    # Using Gemini 2.5 Flash for rapid chatbot responses
    model = genai.GenerativeModel(
        model_name="gemini-2.5-flash",
        system_instruction=SYSTEM_PROMPT
    )
    # We create a chat session to maintain conversation history
    return model.start_chat(history=[])

# ==========================================
# TELEGRAM POLLING LOOP
# ==========================================
def get_updates(offset=None):
    url = f"https://api.telegram.org/bot{TELEGRAM_TOKEN}/getUpdates"
    # timeout=30 enables "Long Polling" so it instantly gets messages without spamming API requests
    params = {"timeout": 30, "offset": offset}
    try:
        r = requests.get(url, params=params)
        return r.json()
    except Exception as e:
        print(f"Error fetching updates: {e}")
        return None

def send_message(chat_id, text):
    url = f"https://api.telegram.org/bot{TELEGRAM_TOKEN}/sendMessage"
    # Send as raw text to avoid Telegram's strict Markdown crashing the API
    payload = {"chat_id": chat_id, "text": text}
    try:
        res = requests.post(url, json=payload)
        if res.status_code != 200:
            print(f"[Telegram Error] {res.text}")
    except Exception as e:
        print(f"Error sending message: {e}")

def main():
    print("[BOT] Starting Senor Medicine Assistant...")
    if GEMINI_API_KEY == "PUT_YOUR_GEMINI_API_KEY_HERE":
        print("[ERROR] Please add your Gemini API Key before running!")
        return

    chat_session = setup_gemini()
    update_id = None
    
    print("[BOT] Listening for patient messages...")
    
    while True:
        updates = get_updates(offset=update_id)
        
        if updates and updates.get("ok"):
            for item in updates["result"]:
                # Update the offset so we don't process the same message twice
                update_id = item["update_id"] + 1
                
                # Check if it's a text message
                message = item.get("message")
                if message and "text" in message:
                    chat_id = message["chat"]["id"]
                    user_text = message["text"]
                    user_name = message["from"].get("first_name", "Patient")
                    
                    print(f"\n[Patient {user_name}] {user_text}")
                    
                    # Handle /start command
                    if user_text.lower() == "/start":
                        welcome = (f"Hello {user_name}! 👋 I am your Smart Medicine Assistant.\n\n"
                                   "I can help you understand your medications, answer basic health questions, "
                                   "or explain disease symptoms. How can I help you today?")
                        send_message(chat_id, welcome)
                        continue
                    
                    # Send typing indicator (optional but gives a nice UX)
                    requests.post(f"https://api.telegram.org/bot{TELEGRAM_TOKEN}/sendChatAction", 
                                  json={"chat_id": chat_id, "action": "typing"})
                    
                    # Ask Gemini
                    try:
                        print("[Bot] Thinking...")
                        response = chat_session.send_message(user_text)
                        
                        # Send the AI response back to Telegram
                        send_message(chat_id, response.text)
                        print("[Bot] Replied.")
                        
                    except Exception as e:
                        print(f"[Error calling Gemini: {e}]")
                        send_message(chat_id, "I'm sorry, my medical database is temporarily unavailable. Please try again in a moment!")
                        
        time.sleep(1) # Small delay to prevent CPU spiking

if __name__ == '__main__':
    main()
