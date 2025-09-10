// src/types/switchgear.ts
export interface Switchgear {
  id: string;                 // internal editor id
  kind: "switchgear";         // discriminator
  title: string;              // "Switchgear #1"
  doUnitId: string | null;
  doOpenCh: number | null;
  doCloseCh: number | null;
  diUnitId: string | null;
  diOpenPulseCh: number | null;
  diClosePulseCh: number | null;
  feedbackDelayMs: number;    // keep it here to bind two-way if needed
}
