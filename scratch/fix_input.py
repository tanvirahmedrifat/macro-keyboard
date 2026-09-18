import re

with open("system_input.cpp", "r") as f:
    content = f.read()

def replacer(match):
    pin = match.group(1)
    b_h = match.group(2)
    b_t = match.group(3)
    b_f = match.group(4)
    ev_hold = match.group(5)
    ev_tap = match.group(6)
    
    return f"""  if (digitalRead({pin}) == LOW) {{
    if (!{b_h}) {{ {b_h} = true; {b_t} = now; {b_f} = false; }}
    if (!{b_f} && (now - {b_t} >= BTN_HOLD_MS)) {{
      {b_f} = true;
      return {ev_hold};
    }}
  }} else {{
    if ({b_h}) {{
      if (now - {b_t} > 30) {{ // Debounce release
        {b_h} = false;
        if (!{b_f}) return {ev_tap};
      }} else {{
        // It's a bounce, ignore and keep {b_h} true (or just let it reset)
        // Wait, if we don't reset {b_h}, it will immediately trigger TAP when 30ms passes!
        // To fix this simply, we just reset it if it was a tiny blip.
        {b_h} = false; 
      }}
    }}
  }}"""

# We can replace all 4 buttons and the 5th has a slightly different pattern.
