import { http } from "./http"
import { API_V1 } from "./utils"
import type {
	Switchgear,
	SwitchgearCreateInput,
	SwitchgearUpdateInput,
} from "@/types/switchgear"

const basePath = (workspaceId: number | string) => `${API_V1}/workspaces/${workspaceId}/switchgears`

export const SwitchgearsAPI = {
	list(workspaceId: number | string) {
		return http.get<Switchgear[]>(basePath(workspaceId))
	},

	get(workspaceId: number | string, id: number | string) {
		return http.get<Switchgear>(`${basePath(workspaceId)}/${id}`)
	},

	create(workspaceId: number | string, payload: SwitchgearCreateInput) {
		return http.post<Switchgear>(basePath(workspaceId), payload)
	},

	update(workspaceId: number | string, id: number | string, payload: SwitchgearUpdateInput) {
		return http.patch<Switchgear>(`${basePath(workspaceId)}/${id}`, payload)
	},

	delete(workspaceId: number | string, id: number | string) {
		return http.delete(`${basePath(workspaceId)}/${id}`)
	},
}

