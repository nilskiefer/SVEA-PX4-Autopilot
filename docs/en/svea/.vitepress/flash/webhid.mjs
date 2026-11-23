const REPORT_SIZE = 64;
const RESPONSE_TIMEOUT = 3000;

function isDapReport(report) {
  const bits = report.items.reduce((size, item) => size + item.reportSize * item.reportCount, 0);
  return bits === REPORT_SIZE * 8;
}

export class CmsisDapHidTransport {
  packetSize = REPORT_SIZE;
  connectDeadline = Infinity;
  #inputReport;
  #outputReport;
  #response;
  #ownsDevice = false;

  constructor(device) {
    this.device = device;
  }

  async open() {
    if (this.device.opened) {
      throw new Error("CODEGRIP is already open. Close the other flashing tool and reconnect.");
    }

    const pairs = [];
    const collections = [...this.device.collections];
    for (const collection of collections) {
      collections.push(...collection.children);
      const inputs = collection.inputReports.filter(isDapReport);
      const outputs = collection.outputReports.filter(isDapReport);
      if (inputs.length === 1 && outputs.length === 1) {
        pairs.push([inputs[0].reportId, outputs[0].reportId]);
      }
    }
    if (pairs.length !== 1) {
      throw new Error("CODEGRIP did not expose a CMSIS-DAP HID report pair.");
    }

    [this.#inputReport, this.#outputReport] = pairs[0];
    await this.device.open();
    this.#ownsDevice = true;
  }

  async close() {
    if (this.#ownsDevice && this.device.opened) {
      await this.device.close();
    }
    this.#ownsDevice = false;
  }

  async write(data) {
    if (performance.now() >= this.connectDeadline) {
      throw new Error("Timed out connecting to the STM32 over SWD.");
    }
    const packet = new Uint8Array(REPORT_SIZE);
    packet.set(data);

    let receive;
    let timer;
    const response = new Promise((resolve, reject) => {
      receive = ({ reportId, data: report }) => {
        if (reportId === this.#inputReport) {
          if (report.byteLength !== REPORT_SIZE) {
            reject(new Error("CODEGRIP returned an unexpected HID report size."));
          } else {
            const data = new Uint8Array(report.buffer, report.byteOffset, report.byteLength);
            resolve(new DataView(data.slice().buffer));
          }
        }
      };
      timer = setTimeout(() => reject(new Error("CODEGRIP did not respond.")), RESPONSE_TIMEOUT);
      this.device.addEventListener("inputreport", receive);
    });

    try {
      const [, report] = await Promise.all([
        this.device.sendReport(this.#outputReport, packet),
        response,
      ]);
      this.#response = report;
    } finally {
      clearTimeout(timer);
      this.device.removeEventListener("inputreport", receive);
    }
  }

  async read() {
    return this.#response;
  }
}
