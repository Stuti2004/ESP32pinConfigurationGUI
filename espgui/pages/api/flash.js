import { spawn } from "child_process";

export default function handler(req, res) {
  // Adjust path to your firmware folder
  const firmwarePath = "C:/Users/ASUS/OneDrive/Documents/GUI/PlatformIO/Projects/firmware";

  const pio = spawn("pio", ["run", "--target", "upload"], {
    cwd: firmwarePath,
    shell: true,
  });

  let output = "";

  pio.stdout.on("data", (data) => {
    output += data.toString();
  });

  pio.stderr.on("data", (data) => {
    output += data.toString();
  });

  pio.on("close", (code) => {
    res.status(200).json({
      success: code === 0,
      logs: output,
    });
  });
}
