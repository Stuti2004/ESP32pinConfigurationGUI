import { Geist, Geist_Mono } from "next/font/google";
import "./globals.css";

const geistSans = Geist({
  variable: "--font-geist-sans",
  subsets: ["latin"],
});

const geistMono = Geist_Mono({
  variable: "--font-geist-mono",
  subsets: ["latin"],
});

export const metadata = {
  title: "ESP32 AHU Controller Configurator",
  description: "Configure your ESP32-based AHU controller with ease. Set up pin functions, GPIO assignments, and Firebase integration all from a user-friendly interface.",
};

export default function RootLayout({ children }) {
  return (
    <html lang="en">
      <body
        className={`${geistSans.variable} ${geistMono.variable} antialiased bg-(--color-bg) text-(--color-text)`}
      >
        <div className="min-h-screen flex flex-col">
          <header className="bg-(--color-primary) text-white p-4 shadow-md">
            <h1 className="text-xl font-bold">
              ESP32 AHU Controller Configurator
            </h1>
          </header>
          <main className="flex-1 p-6">{children}</main>
          {/* <footer className="bg-[var(--color-secondary)] text-white p-3 text-center">
            © {new Date().getFullYear()} ESP32 Configurator
          </footer> */}
        </div>
      </body>
    </html>
  );
}

