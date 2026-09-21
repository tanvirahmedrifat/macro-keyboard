import re

with open("Macro-Keyboard.ino", "r") as f:
    content = f.read()

# Replace delay(...) with MDELAY(...) only between line 935 and 1115 (approx)
# We'll just look for // ╔═══════════════════════════════════════════════════╗
start_idx = content.find("// ╔═══════════════════════════════════════════════════╗")
end_idx = content.find("} // end loop()")

if end_idx == -1: # fallback
    end_idx = content.rfind("}")

top_part = content[:start_idx]
macro_part = content[start_idx:end_idx]
bottom_part = content[end_idx:]

# Define MDELAY at the top of loop()
loop_idx = top_part.rfind("void loop() {")
top_part = top_part[:loop_idx+13] + "\n  #define MDELAY(x) if(macroDelay(x)) { ble.releaseAll(); resetIdle(); goto ABORT_MACRO; }\n  macroAborted = false;\n  macroStartMs = millis();\n" + top_part[loop_idx+13:]

# Replace delays in macro_part
macro_part = re.sub(r'\bdelay\s*\(([^)]+)\)', r'MDELAY(\1)', macro_part)

# Add ABORT_MACRO label at the end
bottom_part = "\nABORT_MACRO:\n  b1h = b2h = b3h = b4h = b5h = false;\n  return;\n" + bottom_part

with open("Macro-Keyboard.ino", "w") as f:
    f.write(top_part + macro_part + bottom_part)
