// eslint.config.js
import js from '@eslint/js'
import vue from 'eslint-plugin-vue'
import typescript from '@typescript-eslint/eslint-plugin'
import parserTs from '@typescript-eslint/parser'
import prettier from 'eslint-plugin-prettier'

export default [
  {
    files: ['**/*.{js,ts,vue}'],
    ignores: ['dist', 'coverage', '**/*.d.ts'],
    languageOptions: {
      parser: parserTs,
      parserOptions: {
        ecmaVersion: 'latest',
        sourceType: 'module',
        extraFileExtensions: ['.vue']
      },
      globals: js.configs.recommended.languageOptions.globals
    },
    plugins: {
      vue,
      '@typescript-eslint': typescript,
      prettier
    },
    rules: {
      ...js.configs.recommended.rules,
      ...typescript.configs.recommended.rules,
      ...vue.configs.base.rules,
      ...vue.configs['vue3-recommended'].rules,
      'prettier/prettier': 'warn',
      'vue/multi-word-component-names': 'off'
    }
  }
]
