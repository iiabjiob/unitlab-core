export interface Workspace {
  id: number
  uuid: string
  name: string
  slug: string
  created_at: string
  updated_at: string
}

export interface WorkspaceCreateInput {
  name: string
  slug: string
}

export interface WorkspaceUpdateInput {
  name?: string
  slug?: string
}
