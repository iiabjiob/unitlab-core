export type TreeviewNode<Value = string> = Readonly<{
  value: Value
  parent: Value | null
  disabled?: boolean
  text?: string
}>
