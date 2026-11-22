/** Copy for the Crowd Counter web flasher. Project identity from firmware/index.json overrides `project`. */
export const site = {
  project: {
    name: "Crowd Counter",
    tagline: "A pocket attendance estimate for the settle-up",
    description:
      "Estimates how many people are in the room by passively counting nearby phones and wearables. An estimate, not a certified count.",
  },
  /** Source link for forks: REPO_URL, else GitHub Actions' repository, else none. */
  repository:
    process.env.REPO_URL ||
    (process.env.GITHUB_REPOSITORY
      ? `https://github.com/${process.env.GITHUB_REPOSITORY}`
      : undefined),
  noRelease:
    "No release published yet. Build locally with pio run -e sticks3, or publish a release to enable browser install.",
  privacyNote:
    "Receive only: the device never connects to, pings or transmits to any device. Addresses are salted and hashed on receipt, never stored by the application or written to flash or serial, and logs hold aggregate counts only; ask the venue's permission first.",
  features: [
    {
      title: "One number at a glance",
      body: "A large estimated count with a status bar. Grey is below 80% of X, amber is 80% to 99%, and green is X or above.",
    },
    {
      title: "An estimate, not a count",
      body: "Use it as a cross-check on the door tally. The detail screen shows the raw counts it is based on.",
    },
    {
      title: "Privacy by design",
      body: "Receive only. Nothing connects to or transmits to a device. Addresses are salted and hashed on receipt, and the salt is held only in RAM, never written to flash or printed, and is gone at power-off. A new salt is generated at every cold power-on and at the end of each night. The ids are pseudonyms: someone with physical debug access to a running device could in principle recover addresses, but nothing stored or exported identifies a device. No core dumps are written, and night logs hold aggregate counts only.",
    },
    {
      title: "Night log and CSV export",
      body: "One row per minute, with the newest 20 nights kept. Export over USB with scripts/export_csv.py from the repository, or send export from any serial terminal and capture the text between BEGIN and END. The summary shows the peak and the minutes at or above X.",
    },
    {
      title: "Calibration by venue type",
      body: "Enter a manual headcount at a full-crowd show. The device fits a ratio for each venue type from its saved samples.",
    },
    {
      title: "Battery, honestly",
      body: "The internal cell is a small 250 mAh. Plan on a USB-C power bank for a full gig. Runtime has not been measured yet.",
    },
  ],
  steps: [
    "Connect the StickS3 with a USB data cable.",
    "Press Install.",
    "Pick the serial port in the browser prompt and confirm.",
    "Wait for the flash to finish. Settings and night logs stay unless you choose to erase.",
  ],
  gigSteps: [
    "Ask the venue's permission first, and be upfront that the device is running.",
    "Power on at load-in. Choose New night, or Resume if prompted. If nobody presses a button within the prompt's timeout, a new night starts.",
    "Set X in settings. KEY1 long press opens settings. In settings, KEY2 adds one step on a press and takes one step off on a double-click.",
    "At soundcheck, run setup mode with the band's and staff's phones nearby: hold KEY2, then hold it again when the screen says to. It finalises after a full averaging window, so you can walk away.",
    "Place it centrally, not on an amp, and power it from a USB-C power bank.",
    "Glance at the number between songs. It is an estimate (est.). The grey, amber and green bar shows how close it is to X.",
    "After the show, export the night log over USB: use scripts/export_csv.py from the repository, or any serial terminal that sends export and saves the text between BEGIN and END.",
  ],
  faq: [
    {
      q: "Which browsers work?",
      a: "Browsers with Web Serial: Chrome, Edge and Opera on desktop. Safari and Firefox cannot flash.",
    },
    {
      q: "The port does not appear.",
      a: "Use a USB data cable and close any serial monitor. Then hold the download-mode button while plugging in.",
    },
    {
      q: "How accurate is it?",
      a: "It is an estimate. The target is within ±20% of a manual count at three real shows after calibration. That is not measured yet. Calibrate with manual counts at full-crowd shows. It counts Bluetooth LE advertisements and 2.4 GHz Wi-Fi probe requests only, so Bluetooth Classic devices and 5 GHz probes are missed, and phones in airplane mode are invisible.",
    },
    {
      q: "Is it legal? What about privacy?",
      a: "It is receive only: it never connects to, pings or disrupts a device, and nothing transmits. No addresses are stored by the application or printed, and logs hold aggregate counts. Rules differ by state and country. This is not legal advice. Ask the venue before you use it.",
    },
    {
      q: "How long does the battery last?",
      a: "The internal cell is small. The design estimate is 1.5 to 2.5 hours with both radios receiving. That is not measured. For a full gig, use a USB-C power bank. Battery voltage is logged every minute, so runtime will be measured.",
    },
    {
      q: "Does a reflash erase my nights?",
      a: "No. Flashing keeps settings and night logs unless you choose to erase. Export first to be safe: scripts/export_csv.py in the repository, or any serial terminal that sends export and saves the text between BEGIN and END.",
    },
    {
      q: "How do I recover a bad flash?",
      a: "Reflash from this page and choose to erase. Or run pio run -e sticks3 -t erase, then pio run -e sticks3 -t upload. Erasing deletes night logs and calibration.",
    },
  ],
};
