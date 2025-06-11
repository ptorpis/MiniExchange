import uuid


VSD = {                     # it stands for Very Secure Database
    "alice": "pwdalice",
    "bob": "pwdbob",
    "testuser": "test"
}


class SessionManager:
    def __init__(self, user_db: dict):
        self.sessions = {}      # map session tokens to users, primary lookup
        self.user_tokens = {}   # map users to users
        self.user_db = user_db

    def login(self, username: str, password: str) -> str | None:
        if username in self.user_db and self.user_db[username] == password:
            if username in self.user_tokens:
                # already logged in
                return self.user_tokens[username]

            token = str(uuid.uuid4())[:8]  # simplified for ease of use
            self.sessions[token] = username
            self.user_tokens[username] = token
            return token
        return None  # login failed

    def logout(self, token: str) -> bool:
        if token in self.sessions:
            username = self.sessions.pop(token)
            self.user_tokens.pop(username, None)
            return True  # logout success
        return False  # logout fail

    def get_user(self, token: str) -> str | None:
        return self.sessions.get(token)
