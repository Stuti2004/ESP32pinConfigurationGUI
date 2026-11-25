"use client";
import { v4 as uuidv4 } from "uuid";
import { useState } from "react";
import PinFunctionRow from "../components/PinFunctionRow";

const ALL_PREDEFINED_PINS = [
  "AHU1_RELAY_PIN", "AHU2_RELAY_PIN", "DAC1_PIN", "DAC2_PIN",
  "DP1_SENSOR_PIN", "DP2_SENSOR_PIN", "Red_RGB", "Blue_RGB", "Green_RGB"
];

export default function ConfigForm() {
  const [rows, setRows] = useState([
    { id: uuidv4(), pin: "", func: "", gpio: "", isCustom: false }
  ]);
  
  const [firebaseHost, setFirebaseHost] = useState("");
  const [firebaseAuthToken, setFirebaseAuthToken] = useState("");
  const [error, setError] = useState("");
  const [consoleOutput, setConsoleOutput] = useState("");

  const handleRowChange = (id, updatedRow) => {
    setRows(rows.map((row) => (row.id === id ? updatedRow : row)));
  };

  const handleAddRow = () => {
    if (rows.length < ALL_PREDEFINED_PINS.length + 10) { 
      setRows([...rows, { id: uuidv4(), pin: "", func: "", gpio: "", isCustom: false }]);
    }
  };

  const handleAddCustomPin = () => {
    setRows([...rows, { id: uuidv4(), pin: "", func: "", gpio: "", isCustom: true }]);
  };

  const handleRemoveRow = (id) => {
    if (rows.length > 1) {
      setRows(rows.filter((row) => row.id !== id));
    } else {
      setError("At least one pin configuration is required.");
      setTimeout(() => setError(""), 3000);
    }
  };

  const handleSubmit = async () => {
    if (!firebaseHost || rows.some(r => !r.pin || !r.func || !r.gpio)) {
      setError("Please fill all fields before submitting.");
      return;
    }

    setError("");
    try {
      const res = await fetch("/api/configure", { 
        method: "POST", 
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ pins: rows, firebaseHost, firebaseAuthToken })
      });
      const data = await res.json();
      setConsoleOutput(prev => prev + "\nConfig saved: " + JSON.stringify(data));
    } catch (err) {
      setError("Failed to save configuration.");
    }
  };

  async function handleFlash() {

    const response = await fetch("/api/flash");

    const reader = response.body.getReader();

    const decoder = new TextDecoder();



    while (true) {

      const { done, value } = await reader.read();

      if (done) break;

      const chunk = decoder.decode(value);

      setConsoleOutput(prev => prev + chunk);

    }



    alert("Flashing complete!");

  }

  const usedPins = rows.map(r => r.pin).filter(Boolean);

  return (
    <div className="max-w-3xl mx-auto p-6 bg-white shadow-xl rounded-xl text-gray-800 mt-10 border border-gray-100">
      <h1 className="text-2xl font-bold mb-6 text-blue-900 border-b pb-4">AHU Controller Configurator</h1>
      <h3 className="font-bold text-xs uppercase tracking-wider text-slate-500">Pins Configuration</h3>
      <div className="space-y-3 mb-6">
        {rows.map((row) => (
          <PinFunctionRow
            key={row.id}
            id={row.id}
            row={row}
            onChange={handleRowChange}
            onRemove={handleRemoveRow}
            availablePins={ALL_PREDEFINED_PINS.filter(p => !usedPins.includes(p) || p === row.pin)}
          />
        ))}
      </div>

      <div className="flex gap-3 mb-8">
        <button onClick={handleAddRow} className="bg-emerald-600 text-white px-5 py-2 rounded-lg hover:bg-emerald-800 transition font-medium shadow-sm">
          + Add Row
        </button>
        <button onClick={handleAddCustomPin} className="bg-amber-500 text-white px-5 py-2 rounded-lg hover:bg-amber-700 transition font-medium shadow-sm">
          + Custom Pin
        </button>
      </div>

      <div className="grid grid-cols-1 gap-4 bg-slate-50 p-5 rounded-lg mb-6 border border-slate-200">
        <h3 className="font-bold text-xs uppercase tracking-wider text-slate-500">Firebase Connectivity</h3>
        <input
          type="text"
          placeholder="Firebase Host"
          className="border p-2.5 w-full rounded-md focus:ring-2 focus:ring-blue-400 outline-none"
          value={firebaseHost}
          onChange={(e) => setFirebaseHost(e.target.value)}
        />
        <input
          type="text"
          placeholder="Authorization Token"
          className="border p-2.5 w-full rounded-md focus:ring-2 focus:ring-blue-400 outline-none"
          value={firebaseAuthToken}
          onChange={(e) => setFirebaseAuthToken(e.target.value)}
        />
      </div>

      {error && <p className="text-red-500 font-medium mb-4 italic">{error}</p>}

      <div className="flex gap-4">
        <button onClick={handleSubmit} className="flex-1 bg-blue-700 text-white py-3 rounded-lg font-bold hover:bg-blue-900 shadow-md">
          Generate Firmware
        </button>
        <button onClick={handleFlash} className="flex-1 bg-purple-700 text-white py-3 rounded-lg font-bold hover:bg-purple-900 shadow-md">
          Flash ESP32
        </button>
      </div>

      <pre className="mt-6 p-4 bg-gray-900 text-green-400 rounded-lg text-xs font-mono h-40 overflow-y-auto">
        {consoleOutput || "// Console output will appear here..."}
      </pre>
    </div>
  );
}