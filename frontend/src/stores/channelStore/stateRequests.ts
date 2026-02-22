import { useWebSocketStore } from "@/stores/websocketStore"
import { WSAction, ReqStateMode, type RequestStateMessage } from "@/types/ws/messages"

type LoggerLike = {
  info: (message: string) => void
  error: (message: string) => void
}

type DeviceLike = {
  id: number
  unit_id: string
  device_type: string
}

type Params = {
  logger: LoggerLike
  findDeviceById: (deviceId: number) => DeviceLike | undefined
}

export function createChannelStateRequests(params: Params) {
  function requestStates(
    deviceId: number,
    options: { includeDiagnostics?: boolean; silent?: boolean } = {},
  ) {
    const {
      includeDiagnostics = true,
      silent = false,
    } = options

    const device = params.findDeviceById(deviceId)
    if (!device) {
      params.logger.error(`Device ${deviceId} not found for requestStates`)
      return
    }

    const ws = useWebSocketStore()
    const lowerType = device.device_type.toLowerCase()
    const msg: RequestStateMessage = {
      action: WSAction.GET_STATES,
      unit_id: device.unit_id,
      mode:
        lowerType === "ao"
          ? ReqStateMode.REQ_ALL_FLOAT
          : ReqStateMode.REQ_ALL_BIT,
    }
    ws.send(msg)
    if (!silent) {
      params.logger.info(`Requested states from ${device.unit_id}`)
    }

    if (includeDiagnostics && lowerType === "do") {
      ws.send({
        action: WSAction.GET_STATES,
        unit_id: device.unit_id,
        mode: ReqStateMode.REQ_DIAG_ALL_BIT,
      } satisfies RequestStateMessage)
      if (!silent) {
        params.logger.info(`Requested DO diagnostics from ${device.unit_id}`)
      }
    } else if (includeDiagnostics && lowerType === "di") {
      ws.send({
        action: WSAction.GET_STATES,
        unit_id: device.unit_id,
        mode: ReqStateMode.REQ_DIAG_DI_BIT,
      } satisfies RequestStateMessage)
      if (!silent) {
        params.logger.info(`Requested DI diagnostics from ${device.unit_id}`)
      }
    }
  }

  return {
    requestStates,
  }
}
