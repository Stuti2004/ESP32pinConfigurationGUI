// pages/api/configure.js
import fs from "fs";
import path from "path";

export default function handler(req, res) {
  if (req.method === "POST") {
    try {
      const config = req.body;

      let header = "";
      config.pins.forEach(p => {
        header += `#define ${p.pin} ${p.gpio}\n`;
      });
      header += `#define FIREBASE_HOST "${config.firebaseHost}"\n`;
      header += `#define FIREBASE_AUTH "${config.firebaseAuthToken}"\n`;

      
      const configPath = path.join(process.cwd(), "../PlatformIO/Projects/firmware/include/config.h");
      fs.mkdirSync(path.dirname(configPath), { recursive: true });
      fs.writeFileSync(configPath, header);

      res.status(200).json({ message: "Config.h generated successfully" });
    } catch (err) {
      console.error("Error writing config.h:", err);
      res.status(500).json({ error: "Failed to generate config.h" });
    }
  } else {
    res.status(405).json({ error: "Method not allowed" });
  }
}
