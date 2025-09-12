export interface Switchgear {
  id: number
  kind: "switchgear" | "disconnector" | "earthing"   // можно расширять
  title: string

  // тут просто id канала, а детали резолвятся через channelStore
  do_open: number | null
  do_closed: number | null
  di_open: number | null
  di_close: number | null

  feedback_delay_ms: number
}
