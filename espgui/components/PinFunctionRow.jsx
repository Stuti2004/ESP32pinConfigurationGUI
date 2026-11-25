"use client";

const functionOptions = [
  "Digital I/O", "Input‑Only", "Analog Inputs (ADC)", "Relay Outputs",
  "DAC Output", "UART", "SPI", "I²C", "Modbus_RS485",
  "VoltageInputs", "VoltageOutputs", "PWMOutputs"
];

const gpioMap = {
  "Digital I/O": [4, 5, 12, 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33],
  "Input‑Only": [34, 35, 36, 39],
  "Analog Inputs (ADC)": [32, 33, 34, 35, 36, 39],
  "Relay Outputs": [4, 5, 12, 13, 14, 16, 17, 18, 19, 21, 22, 23, 27, 32, 33],
  "DAC Output": [25, 26],
  "UART": [1, 3, 16, 17],
  "SPI": [5, 18, 19, 23],
  "I²C": [21, 22],
  "Modbus_RS485": [16, 17],
  "VoltageInputs": [32, 33, 34, 35, 36, 39],
  "VoltageOutputs": [25, 26],
  "PWMOutputs": [4, 5, 12, 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33]
};

export default function PinFunctionRow({ id, row, onChange, onRemove, availablePins }) {
  
  const handleChange = (field, value) => {
    const updates = { ...row, [field]: value };
    if (field === "func") updates.gpio = ""; 
    onChange(id, updates);
  };

  const validGpios = row.func ? gpioMap[row.func] || [] : [];

  return (
    <div className="flex gap-2 items-center bg-white p-1 rounded-md shadow-sm">
      {/* Pin Name Section */}
      {row.isCustom ? (
        <input
          type="text"
          placeholder="Pin Name"
          className="border p-2 rounded w-1/3 text-sm"
          value={row.pin}
          onChange={(e) => handleChange("pin", e.target.value)}
        />
      ) : (
        <select
          className="border p-2 rounded w-1/3 text-sm bg-gray-50"
          value={row.pin}
          onChange={(e) => handleChange("pin", e.target.value)}
        >
          <option value="">Select Pin</option>
          {availablePins.map((p) => (
            <option key={p} value={p}>{p}</option>
          ))}
        </select>
      )}

      {/* Function Selection */}
      <select
        className="border p-2 rounded w-1/3 text-sm"
        value={row.func}
        onChange={(e) => handleChange("func", e.target.value)}
      >
        <option value="">Select Function</option>
        {functionOptions.map((f) => (
          <option key={f} value={f}>{f}</option>
        ))}
      </select>

      {/* GPIO Selection */}
      <select
        className="border p-2 rounded w-1/4 text-sm"
        value={row.gpio}
        onChange={(e) => handleChange("gpio", e.target.value)}
        disabled={!row.func}
      >
        <option value="">GPIO</option>
        {validGpios.map((g) => (
          <option key={g} value={g}>{`GPIO ${g}`}</option>
        ))}
      </select>
      <button
        onClick={() => onRemove(id)}
        className="text-white hover:bg-red-400 bg-red-600 p-2 transition"
        title="Remove Pin"
      >
        <svg xmlns="http://www.w3.org/2000/svg" className="h-5 w-5" viewBox="0 0 20 20" fill="currentColor">
          <path fillRule="evenodd" d="M4.293 4.293a1 1 0 011.414 0L10 8.586l4.293-4.293a1 1 0 111.414 1.414L11.414 10l4.293 4.293a1 1 0 01-1.414 1.414L10 11.414l-4.293 4.293a1 1 0 01-1.414-1.414L8.586 10 4.293 5.707a1 1 0 010-1.414z" clipRule="evenodd" />
        </svg>
      </button>
    </div>
  );
}