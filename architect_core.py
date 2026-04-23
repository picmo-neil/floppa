import sys, os, re, requests, time, subprocess, random

# FORCE UNBUFFERED OUTPUT (Real-time logging)
sys.stdout.reconfigure(line_buffering=True)

# --- CONFIGURATION LAYER ---
RAW_POOL = os.environ.get("KEY_POOL", "").replace(" ", "").replace("\n", "")
KEY_LIST = [k.strip() for k in RAW_POOL.split(",") if k.strip()]

M1 = os.environ.get("MODEL_PRI", "gemini-3-flash-preview").replace("models/", "")
M2 = os.environ.get("MODEL_SEC", "gemini-2.5-flash").replace("models/", "")

# --- STATE MANAGEMENT ---
CURRENT_KEY_IDX = 0
CURRENT_MODEL_MODE = 1 # 1=Primary, 2=Fallback

# --- PHYSICS ENGINE (Rate Limit Avoidance) ---
# Context: 300 Pre + 300 Post = 600 Lines
CTX_LINES = 300

# Refill Rate: Characters per second allowed (Conservative estimate)
REFILL_RATE_CHARS = 200.0 
MIN_SLEEP_BASE = 5.0

print(f">>> [ARCHITECT] System Online.")
print(f"    - Arsenal: {len(KEY_LIST)} Keys")
print(f"    - Primary: {M1} | Fallback: {M2}")

if not KEY_LIST:
    print("❌ FATAL: No API Keys provided.")
    sys.exit(1)

# ======================================================
# CLASS: RESOURCE MANAGER (Hydra + Phoenix)
# ======================================================
class ResourceManager:
    @staticmethod
    def get_active_model():
        return M1 if CURRENT_MODEL_MODE == 1 else M2

    @staticmethod
    def get_active_key():
        return KEY_LIST[CURRENT_KEY_IDX % len(KEY_LIST)]

    @staticmethod
    def rotate_or_escalate():
        global CURRENT_KEY_IDX, CURRENT_MODEL_MODE, KEY_LIST
        
        # 1. Rotate Key
        CURRENT_KEY_IDX = (CURRENT_KEY_IDX + 1)
        
        # 2. Check if we cycled through all keys
        if CURRENT_KEY_IDX >= len(KEY_LIST):
            CURRENT_KEY_IDX = 0 # Reset index
            
            # If we were on Primary, Escalation to Fallback
            if CURRENT_MODEL_MODE == 1:
                print(f"    🔥 [PHOENIX] Primary Model Exhausted. Escalating to Fallback: {M2}")
                CURRENT_MODEL_MODE = 2
                print("    💤 System Cool-down (20s)...")
                time.sleep(20)
                return True
            else:
                print("    💀 CRITICAL: All Resources Depleted.")
                return False
        
        print(f"    🔄 [HYDRA] Rotating to Key #{CURRENT_KEY_IDX+1}...")
        return True

# ======================================================
# CLASS: PHYSICS GOVERNOR (Rate Limiter)
# ======================================================
class PhysicsGovernor:
    @staticmethod
    def pace(payload_text):
        w = len(payload_text)
        # Dynamic sleep: (Chars / Rate) + Base + Jitter
        wait = (w / REFILL_RATE_CHARS) + MIN_SLEEP_BASE
        wait += random.uniform(0.5, 2.0) # Add jitter to prevent synchronized blocks
        
        wait = min(wait, 120.0) # Cap at 120s
        
        # Relax pacing if using Fallback (Lite models usually faster)
        if CURRENT_MODEL_MODE == 2: wait *= 0.8

        print(f"    ⏳ Physics: {w} chars -> Pacing {wait:.1f}s...")
        time.sleep(wait)

# ======================================================
# CLASS: HEURISTIC ENGINE (Cortex)
# ======================================================
class HeuristicEngine:
    @staticmethod
    def solve(local, remote, filename):
        l_lines = [x.strip() for x in local.splitlines() if x.strip()]
        r_lines = [x.strip() for x in remote.splitlines() if x.strip()]

        # 1. Makefile Logic
        if filename == 'Makefile' or filename.endswith('.mk'):
            # Version Bumps -> Always Accept Upstream
            if any(x.startswith(('VERSION', 'PATCHLEVEL', 'SUBLEVEL', 'EXTRAVERSION')) for x in r_lines):
                print(f"    ⚡ [CORTEX] Version Bump -> Remote")
                return remote
            # Assignment Appends (+=) -> Merge Union
            if all('+=' in x for x in l_lines + r_lines):
                print(f"    ⚡ [CORTEX] Makefile Append -> Merge")
                return local + "\n" + remote

        # 2. Lists & Configs (Union)
        is_list = any(filename.endswith(x) for x in ['.gitignore', '.txt', '.conf', '.json'])
        is_include = all(x.startswith(('#include', 'import')) for x in l_lines + r_lines)
        is_kconfig = ('Kconfig' in filename) and \
                     all(re.match(r'^(config|bool|default|select|menu)', x) or '=' in x for x in l_lines + r_lines)

        if is_list or is_include or is_kconfig:
            print(f"    ⚡ [CORTEX] Set Union -> Merge")
            return "\n".join(sorted(list(set(l_lines + r_lines))))

        return None

    @staticmethod
    def detect_ghost(local, remote):
        # Ghost Detection: 
        # Case A: Local Empty, Remote Content -> Local Deleted it -> Keep Deleted.
        if not local.strip() and remote.strip():
            print(f"    👻 [CORTEX] Local Deletion Detected -> Enforcing Deletion")
            return "" 
        
        # Case B: Local Content, Remote Empty -> Remote Deleted it -> Keep Local (Supremacy).
        if local.strip() and not remote.strip():
            print(f"    🛡️ [CORTEX] Remote Deletion Detected -> Enforcing Local Preservation")
            return local

        return None

    @staticmethod
    def validate(text, filename):
        clean = re.sub(r'//.*', '', text)
        clean = re.sub(r'/\*.*?\*/', '', clean, flags=re.DOTALL)
        if any(filename.endswith(x) for x in ['.txt', '.gitignore', 'Makefile', '.mk', 'Kconfig']): return True
        return clean.count('{') == clean.count('}')

# ======================================================
# CLASS: GENERATIVE GATEWAY (Gemini)
# ======================================================
class GenerativeGateway:
    @staticmethod
    def clean_response(text):
        text = re.sub(r'^```[a-z]*\n', '', text, flags=re.MULTILINE)
        text = re.sub(r'\n```$', '', text, flags=re.MULTILINE)
        text = re.sub(r'<<<<<<< .*', '', text)
        text = re.sub(r'=======', '', text)
        text = re.sub(r'>>>>>>> .*', '', text)
        return text.strip()

    @staticmethod
    def query(prompt):
        # Retry loop handled by Resource Manager Rotation
        max_attempts = len(KEY_LIST) * 3 
        attempts = 0

        while attempts < max_attempts:
            key = ResourceManager.get_active_key()
            model = ResourceManager.get_active_model()
            
            # GOVERNOR
            PhysicsGovernor.pace(prompt)

            url = f"https://generativelanguage.googleapis.com/v1beta/models/{model}:generateContent?key={key}"
            headers = {'Content-Type': 'application/json'}
            data = {
                "contents": [{"parts": [{"text": prompt}]}],
                "generationConfig": {
                    "temperature": 0.1, 
                    "maxOutputTokens": 8192
                }
            }

            try:
                resp = requests.post(url, headers=headers, json=data, timeout=120)
                
                if resp.status_code == 200:
                    try:
                        return GenerativeGateway.clean_response(resp.json()['candidates'][0]['content']['parts'][0]['text'])
                    except:
                        print("    ⚠️ JSON Parsing Error. Retrying...")
                
                # RATE LIMIT / QUOTA -> ROTATE
                elif resp.status_code in [429, 403, 503]:
                    print(f"    🛑 Status {resp.status_code} ({model}). Triggering Rotation...")
                    if not ResourceManager.rotate_or_escalate(): return None
                    continue
                
                else:
                    print(f"    ❌ Fatal API Error: {resp.status_code}")
                    return None

            except Exception as e:
                print(f"    ⚠️ Network Exception: {e}")
                time.sleep(5)
            
            attempts += 1
        
        return None

# ======================================================
# MAIN EXECUTION LOOP
# ======================================================
def solve_conflict_file(filepath):
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f: lines = f.readlines()
    except: return

    new_lines = []
    i = 0
    modified = False
    
    while i < len(lines):
        line = lines[i]
        if line.startswith('<<<<<<<'):
            start = i
            
            # MASSIVE CONTEXT (300 Pre / 300 Post)
            pre_start = max(0, start - CTX_LINES)
            pre = "".join(lines[pre_start:start])
            
            local_blk, remote_blk = [], []
            mode = "local"
            i += 1
            while i < len(lines):
                if lines[i].startswith('======='): mode = "remote"
                elif lines[i].startswith('>>>>>>>'): break
                else:
                    if mode == "local": local_blk.append(lines[i])
                    else: remote_blk.append(lines[i])
                i += 1
            
            post_end = min(len(lines), i + 1 + CTX_LINES)
            post = "".join(lines[i+1:post_end])
            
            local_str = "".join(local_blk)
            remote_str = "".join(remote_blk)
            
            print(f"🔎 Conflict: {filepath} (Line {start}) | Ctx: {len(pre)+len(post)} chars")

            # STEP 1: HEURISTIC SOLVER
            merged = HeuristicEngine.solve(local_str, remote_str, filepath)
            
            # STEP 2: GHOST DETECTION
            if merged is None:
                merged = HeuristicEngine.detect_ghost(local_str, remote_str)

            # STEP 3: GENERATIVE AI
            if merged is None:
                prompt = f"""
                ROLE: Senior Linux Kernel Architect.
                TASK: Resolve Git Merge Conflict in '{filepath}'.
                
                **DIRECTIVES:**
                1. **LOCAL SUPREMACY:** The 'LOCAL' block is my modified source code. It takes precedence.
                2. **INTEGRATION:** Only apply 'REMOTE' (Upstream) changes if they are bug fixes or version updates that DOES NOT BREAK Local logic.
                3. **SYNTAX:** Ensure perfect brace/parenthesis balance based on the CONTEXT.
                4. **CLEANLINESS:** Valid C Code ONLY. No Markdown. No Comments.
                
                CONTEXT BEFORE:
                {pre}
                
                <<<<<<< LOCAL
                {local_str}
                =======
                {remote_str}
                >>>>>>> REMOTE
                
                CONTEXT AFTER:
                {post}
                
                OUTPUT: VALID MERGED CODE BLOCK ONLY.
                """
                
                print(f"    🧠 Asking AI ({ResourceManager.get_active_model()})...")
                merged = GenerativeGateway.query(prompt)

            # STEP 4: FINAL VALIDATION
            if merged is not None:
                if HeuristicEngine.validate(merged, filepath):
                    print("    ✅ Resolved & Validated.")
                    new_lines.append(merged + "\n")
                else:
                    print("    ☢️ Validation Failed (Syntax). Enforcing LOCAL.")
                    new_lines.append(local_str)
            else:
                print("    💀 Resolution Failed. Enforcing LOCAL.")
                new_lines.append(local_str)

            modified = True
        else:
            new_lines.append(line)
        i += 1

    if modified:
        with open(filepath, 'w', encoding='utf-8') as f: f.writelines(new_lines)
        subprocess.run(["git", "add", filepath], check=True)

if __name__ == "__main__":
    if not os.path.exists("conflicts.txt"): sys.exit(0)
    with open("conflicts.txt", "r") as f:
        files = [x.strip() for x in f if x.strip()]
    
    print(f"📋 Targets: {len(files)} files.")
    for f in files:
        if os.path.exists(f): 
            solve_conflict_file(f)
