# Sistema de Processamento de Arquivos Grandes — Compressão e Busca por Substring (ED‑II)

> **Versão do projeto:** Final  
> **Linguagem:** C (C11) • **Plataforma alvo:** Windows 10/11 (GCC/MinGW) • **Execução:** CLI (interativa e por parâmetros)  
> **Autores:** Equipe do trabalho (Pedro e grupo) • **Apelido do assistente:** Atlas

---

## Sumário
- [1. Visão Geral](#1-visão-geral)
- [2. Funcionalidades](#2-funcionalidades)
- [3. Como Compilar (Windows / GCC MinGW)](#3-como-compilar-windows--gcc-mingw)
- [4. Como Executar](#4-como-executar)
  - [4.1 Interface Interativa](#41-interface-interativa)
  - [4.2 Interface por Linha de Comando](#42-interface-por-linha-de-comando)
  - [4.3 Flags e Parâmetros Globais](#43-flags-e-parâmetros-globais)
  - [4.4 Gerador de Arquivos de Teste (`tests/gen_story.c`)](#44-gerador-de-arquivos-de-teste-testsgens_storyc)
- [5. Arquitetura & Métodos Utilizados](#5-arquitetura--métodos-utilizados)
  - [5.1 Etapa 1 — Compressão por Blocos (LZW 12‑bit)](#51-etapa-1--compressão-por-blocos-lzw-12bit)
  - [5.2 Etapa 2 — Busca no Original (KMP em Streaming)](#52-etapa-2--busca-no-original-kmp-em-streaming)
  - [5.3 Etapa 3 — Busca no Compactado (Descompressão Seletiva + KMP Incremental)](#53-etapa-3--busca-no-compactado-descompressão-seletiva--kmp-incremental)
  - [5.4 Etapa 4 — Descompactação Completa (Round‑Trip)](#54-etapa-4--descompactação-completa-roundtrip)
  - [5.5 Modelo de Memória Constante](#55-modelo-de-memória-constante)
- [6. Formato do Arquivo `.ed2c`](#6-formato-do-arquivo-ed2c)
  - [6.1 Layout Global](#61-layout-global)
  - [6.2 Cabeçalho (Header)](#62-cabeçalho-header)
  - [6.3 Tabela de Índice](#63-tabela-de-índice)
  - [6.4 Fluxos de Leitura Típicos](#64-fluxos-de-leitura-típicos)
  - [6.5 Observações de Design](#65-observações-de-design)
- [7. Códigos de Retorno (rc) e Soluções Comuns](#7-códigos-de-retorno-rc-e-soluções-comuns)
- [8. Boas Práticas, Desempenho e Tunagem](#8-boas-práticas-desempenho-e-tunagem)
- [9. Perguntas Frequentes (FAQ)](#9-perguntas-frequentes-faq)
- [10. Guia de Defesa: Domínio](#10-guia-de-defesa-domínio)
- [11. Estrutura do Repositório](#11-estrutura-do-repositório)
- [12. Licença e Créditos](#12-licença-e-créditos)

---

## 1. Visão Geral
Este projeto implementa uma **ferramenta CLI** para **compactar arquivos de texto muito grandes** e **buscar substrings** tanto no arquivo **original** quanto **diretamente no compactado**, **sem descompactar tudo**.  
O foco didático é aplicar **algoritmos clássicos** (LZW e KMP), **manipulação de arquivos em baixo nível** e **gestão de memória constante (streaming)** — requisito central da disciplina **Estruturas de Dados II**.

**Pontos‑chave:**  
- **Compressão por blocos com índice** → viabiliza **descompressão seletiva** por região.  
- **KMP incremental** → preserva o estado entre buffers/blocos, encontrando ocorrências que cruzam fronteiras.  
- **Memória O(1)** em relação ao tamanho do arquivo → buffers de tamanho fixo; não acumulamos matches em RAM.  
- **Offsets sempre no arquivo original** → relatórios fáceis de interpretar/validar.

---

## 2. Funcionalidades
- **Compactar** (`.txt → .ed2c`) com **LZW 12‑bit por bloco**.
- **Buscar no original** (KMP streaming) com offsets 0‑based em bytes.
- **Buscar no compactado** (descompressão bloco‑a‑bloco + KMP incremental), reportando **offsets do arquivo original**.
- **Descompactar** (`.ed2c → .txt`) para validar integridade (round‑trip).
- **Relatório** de tempo, memória aproximada (**Working Set**), blocos e razão de compressão.
- **Flags úteis**: `--silent`, `--count-only`, `--out=FILE`.
- **Compatível com arquivos gigantes** (offsets 64‑bit).

---

## 3. Como Compilar (Windows / GCC MinGW)
Pré‑requisitos:
- **GCC/MinGW** instalado e no `PATH` (ex.: via MSYS2 ou WinLibs).

No diretório raiz do projeto (onde há `include/` e `src/`):
```powershell
# Compilar a aplicação principal
gcc -std=c11 -O2 -Wall -Wextra -pedantic -D_FILE_OFFSET_BITS=64 -Iinclude `
  src/main.c src/cli.c src/kmp.c src/index.c src/lzw.c src/sysmon.c `
  -o meu_programa.exe -lpsapi
```

**Explicando rapidamente alguns flags**:
- `-std=c11` → usa C11.  
- `-O2` → otimização geral.  
- `-Wall -Wextra -pedantic` → ativa alertas de boas práticas e portabilidade.  
- `-D_FILE_OFFSET_BITS=64` → offsets 64‑bit (arquivos > 2 GiB).  
- `-Iinclude` → inclui cabeçalhos locais.  
- `-lpsapi` → linka a **PSAPI (Windows)**, usada para medir **Working Set** (memória aproximada do processo).

### Compilando o gerador de arquivos de teste
O projeto inclui um **gerador em C** para criar textos grandes e variados para benchmarks e validação.

```powershell
# A partir da raiz do repositório
gcc -std=c11 -O2 -Wall -Wextra -pedantic -D_FILE_OFFSET_BITS=64 `
  tests/gen_story.c -o gen_story.exe
```

> **Notas (Windows):** o código do gerador usa `_fseeki64/_ftelli64` quando disponível para suportar arquivos enormes. Em toolchains antigos, substituímos por `fseeko/ftello` ou `fseek/ftell` com atenções a 64‑bit.

---

## 4. Como Executar

### 4.1 Interface Interativa
Execute sem parâmetros e use o menu:
```
meu_programa.exe
=== Menu ===
1) Compactar arquivo (Etapa 1)
2) Buscar em arquivo original (Etapa 2)
3) Buscar em arquivo compactado .ed2c (Etapa 3)
4) Descompactar .ed2c para .txt (Etapa 4)
0) Sair
```

### 4.2 Interface por Linha de Comando
Os subcomandos seguem o enunciado do professor:

```
# Etapa 1 — Compactar
meu_programa.exe compactar <in.txt> <out.ed2c> [<blkMiB>]

# Etapa 2 — Buscar em original
meu_programa.exe buscar_simples <in.txt> "<substring>" [--count-only] [--silent] [--out=matches.txt]

# Etapa 3 — Buscar em compactado
meu_programa.exe buscar_compactado <in.ed2c> "<substring>" [--count-only] [--silent] [--out=matches.txt]

# Etapa 4 — Descompactar
meu_programa.exe descompactar <in.ed2c> <out.txt>
```

- `<blkMiB>`: tamanho‑alvo de bloco **descompactado** (MiB). Padrão recomendado: **4**.  
- `--count-only`: apenas contabiliza, **não** imprime offsets.  
- `--silent`: silencia o console (bom p/ benchmarks). Se usar `--out=FILE`, os offsets vão **para o arquivo**.  
- `--out=FILE`: grava offsets em arquivo (se **não** estiver em `--count-only`).

### 4.3 Flags e Parâmetros Globais
- **Relatório final** (tempo/memória/blocos/razão): impresso ao término, **a menos que** `--silent`.  
- **Offsets**: sempre em **bytes do arquivo original** (0‑based).  
- **Padrão vazio** (size 0): rejeitado com erro.

### 4.4 Gerador de Arquivos de Teste (`tests/gen_story.c`)
O gerador cria **textos longos e variados** (fantasia/aventura), com **vocabulário embaralhado**, inserções periódicas de **palavras‑chave** (ex.: “Gideon”, “machado”), e controle por **seed** para reprodutibilidade. Ele é útil para:
- **Estressar** a Etapa 2 (KMP streaming) e Etapa 3 (descompressão seletiva + KMP).  
- Produzir **arquivos gigantes** (de MiB a GiB) sem depender de downloads externos.  
- Medir **impacto de I/O** vs. CPU em cenários reais.

**Uso básico (tamanhos por MiB):**
```powershell
# Gera ~2 GiB de texto com seed fixa (reprodutível)
.\gen_story.exe --out huge.txt --target-mib 2048 --seed 42
```

**Opções principais:**
- `--out <arquivo>`: caminho do `.txt` de saída (obrigatório).  
- `--target-mib <N>`: tamanho aproximado do arquivo em **MiB** (ex.: 8, 256, 2048).  
- `--seed <N>`: semente pseudo‑aleatória (gera o **mesmo** arquivo quando repetida).  
- `--keyword <palavra>` (opcional): insere uma **palavra‑chave** periódica para facilitar testes de busca.  
- `--every <n>` (opcional): a cada **n** frases/parágrafos, injeta a `--keyword` (padrão inteligente se omitido).

**Exemplos:**
```powershell
# 1) Arquivo de ~8 MiB com palavra‑chave "Gideon" a cada ~1000 frases
.\gen_story.exe --out story_8MiB.txt --target-mib 8 --seed 7 --keyword Gideon --every 1000

# 2) Arquivo de ~512 MiB sem palavra‑chave explícita
.\gen_story.exe --out story_512MiB.txt --target-mib 512 --seed 123

# 3) Arquivo de ~2 GiB e depois compactar/buscar
.\gen_story.exe --out huge.txt --target-mib 2048 --seed 42
.\meu_programa.exe compactar huge.txt huge.ed2c 4
.\meu_programa.exe buscar_simples huge.txt "Gideon" --count-only --silent
.\meu_programa.exe buscar_compactado huge.ed2c "Gideon" --count-only --silent
```

**Como funciona (resumo):**
- Parte de **fragments** de história e **tabelas de sinônimos**, combinados por **PRNG** (seed).  
- **Gera frases/periodizações** diferentes a cada execução (com a mesma seed, o texto se repete).  
- Controla o **tamanho** aproximando‑se do alvo em MiB e finaliza ao alcançar o limiar.  
- Pode **injetar tokens** periódicos para facilitar medições e comparação Etapa 2 × Etapa 3.

---

## 5. Arquitetura & Métodos Utilizados

### 5.1 Etapa 1 — Compressão por Blocos (LZW 12‑bit)
- Lê `in.txt` em **blocos de `blk_orig_len`** (ex.: 4 MiB).  
- Para cada bloco, roda **LZW com dicionário reiniciado** e empacota códigos de **12 bits**.  
- Escreve os blocos comprimidos em sequência e, ao fim, escreve a **tabela de índice** (com `orig_off, orig_len, comp_off, comp_len`).  
- Atualiza header com `index_offset`/`index_length` e métricas.

### 5.2 Etapa 2 — Busca no Original (KMP em Streaming)
- Constrói `pi[]` do padrão e lê o arquivo em **buffers fixos**.  
- **Preserva o estado `q` do KMP** ao trocar de buffer → encontra matches que atravessam fronteiras.  
- Em cada match, emite/conta o **offset global**.

### 5.3 Etapa 3 — Busca no Compactado (Descompressão Seletiva + KMP Incremental)
- Carrega **somente** o **índice** em RAM.  
- Itera blocos em ordem de `orig_off`:
  - `seek` em `comp_off`, lê `comp_len`, **descompacta** para um buffer ≤ `blk_orig_len`,  
  - alimenta o **mesmo KMP** (preservando `q` entre blocos),  
  - calcula offsets como `orig_off + pos_local - (m-1)`.

### 5.4 Etapa 4 — Descompactação Completa (Round‑Trip)
- Percorre o índice, lê cada bloco comprimido, **descompacta** e grava no `.txt` de saída.  
- Usado para **verificar integridade** e medir razão de compressão.

### 5.5 Modelo de Memória Constante
- **Streaming**: nunca carregamos o arquivo inteiro; só **um bloco/buffer** por vez.  
- **Sem acumular resultados**: offsets são emitidos no ato (ou para arquivo), `--count-only` guarda apenas um contador.  
- **Índice compacto**: 32 bytes por bloco; com 4 MiB/bloco, 2 GiB ≈ 512 blocos ≈ ~16 KiB de índice.

---

## 6. Formato do Arquivo `.ed2c`

### 6.1 Layout Global
```
[HEADER fixo]  [BLOCOS COMPRIMIDOS ...]  [ÍNDICE (tabela de mapeamento)]
^ offset 0                                 ^ index_offset (vem no header)
```

- **Little‑endian**; tipos fixos (`u16, u32, u64`).  
- **Blocos LZW independentes** (dicionário reinicia): permite **pular** diretamente ao bloco desejado.

### 6.2 Cabeçalho (Header)
Tamanho fixo; contém:

| Campo | Tipo | Descrição |
|---|---|---|
| `magic` | 4 chars | Assinatura `"ED2C"` |
| `version` | `u16` | Versão do formato |
| `flags` | `u16` | Bitmask (ex.: futuro `CRC por bloco`) |
| `algo` | `u32` | Algoritmo (1 = LZW‑12 por blocos) |
| `blk_orig_len` | `u32` | Tamanho‑alvo do bloco descompactado (bytes) |
| `orig_bytes` | `u64` | Tamanho total do original |
| `comp_bytes` | `u64` | Tamanho total comprimido (soma) |
| `index_offset` | `u64` | Offset onde começa a tabela de índice |
| `index_length` | `u64` | Tamanho total da tabela de índice |

> O índice é escrito **no final** porque só então conhecemos todos os `comp_off/comp_len`.

### 6.3 Tabela de Índice
Entrada **fixa de 32 bytes** por bloco:

| Campo | Tipo | Descrição |
|---|---|---|
| `orig_off` | `u64` | Offset (no **original**) onde o bloco começa |
| `orig_len` | `u32` | Tamanho descompactado do bloco |
| `comp_off` | `u64` | Offset no `.ed2c` onde o bloco **comprimido** começa |
| `comp_len` | `u32` | Tamanho comprimido do bloco |
| `crc32` | `u32` | (Reservado) CRC32 do bloco comprimido (0 se inativo) |
| `reserved` | `u32` | Reservado/alinhamento |

### 6.4 Fluxos de Leitura Típicos
- **Buscar no compactado** → ler header/índice → para cada entrada: `seek(comp_off)` → `read(comp_len)` → `decompress` → **KMP** com `orig_off` como base.  
- **Descompactar** → igual, mas escreve `obuf` sequencialmente no `.txt` de saída.

### 6.5 Observações de Design
- **Offsets reportados sempre do original** → consistência entre Etapas 2 e 3.  
- **Tamanho do bloco (`blk_orig_len`)** controla memória e latência:  
  - Maior → melhora compressão, aumenta RAM/latência por bloco.  
  - Menor → piora compressão, reduz RAM, menor latência do 1º match.  
- Índice **pequeno** e **monolítico** simplifica leitura e reduz I/O.

---

## 7. Códigos de Retorno (rc) e Soluções Comuns

| rc | Onde costuma ocorrer | Significa | Ações sugeridas |
|---:|---|---|---|
| **0** | Todos | Sucesso | — |
| **1** | Abrir arquivo de entrada (`fopen in`) | Caminho inválido/permissão | Verifique path, aspas, barras, acesso |
| **2** | Abrir arquivo de saída (`fopen out`) | Não foi possível criar/abrir | Pasta inexistente/permissão/arquivo em uso |
| **3** | Leitura/validação do **header** | `.ed2c` inválido/algoritmo não suportado | Recrie o arquivo, confira versão/assinatura |
| **4** | Alocações (`malloc/realloc`) | Memória insuficiente | Diminua `blkMiB`/feche apps/libere RAM |
| **5** | Leitura do índice ou `fread` | I/O falhou/arquivo corrompido | Verifique disco, integridade do `.ed2c` |
| **6** | `lzw_compress_block` | Falha no compressor | Verifique entrada/bloco; tente outro `blkMiB` |
| **7** | `lseek/_lseeki64` | Falha em posicionar | Path/arquivo corrompido/offset inválido |
| **8** | `write/fwrite` / gravação do índice | I/O falhou ao escrever | Verificar espaço em disco/locks/perm. |
| **9** | `lzw_decompress_block` | Stream inválido/corrompido | Recrie `.ed2c`; verifique header/índice |
| **10** | `fd_full_read` | Leitura incompleta/erro de I/O | Verifique disco/caminho/antivírus interferindo |
| **11** | (reservado) | — | — |
| **12** | Padrão inválido | Padrão vazio (`m==0`) | Forneça substring não vazia |

> Dica: para **debug** em arquivos gigantes, rode com `--count-only --silent` (isola custo de console). Para auditar offsets, use `--out=matches.txt`.

---

## 8. Boas Práticas, Desempenho e Tunagem
- **Impressão de offsets é cara**. Para grandes volumes, prefira `--count-only` ou `--out=FILE` + `--silent`.  
- **Tamanho de bloco (Etapa 1/3)**: 4 MiB é um **excelente meio-termo**. 8–16 MiB podem comprimir melhor (RAM/latência maiores).  
- **Memória**: o Working Set refletirá ~`blk_orig_len` + overhead. Aumente blocos sabendo que a RAM sobe junto.  
- **Comparações Etapa 2 vs Etapa 3**: as contagens devem coincidir; diferenças geralmente decorrem de I/O no console.  
- **Validação**: sempre que possível, **descompacte** (Etapa 4) e compare com o original (hash/contagem).

---

## 9. Perguntas Frequentes (FAQ)

**Q1. Por que a busca no compactado parece mais lenta?**  
Porque além da busca há **descompressão por bloco**. Em testes honestos (sem imprimir milhões de offsets no console), o throughput costuma ser parecido; use `--silent`/`--count-only`.

**Q2. Como o projeto encontra palavras que cortam blocos/buffers?**  
O **estado `q` do KMP** é **preservado** entre buffers (Etapa 2) e entre blocos descompactados (Etapa 3).

**Q3. O que significa “Memória approx … Working Set”?**  
É a **RAM residente** do processo naquele instante (Windows PSAPI). Não é “memória virtual” nem “pico”.

**Q4. Posso mudar o tamanho do bloco depois?**  
Sim, mas cada `.ed2c` codifica o `blk_orig_len` **no header**. Ajuste conforme seu cenário.

**Q5. Os offsets são do arquivo original ou do bloco?**  
**Sempre do original (0‑based)**. Na Etapa 3, calculamos `orig_off + pos_local - (m-1)`.

**Q6. Como gerar arquivos de teste enormes?**  
Use o **gerador `gen_story.exe`** (seção [4.4](#44-gerador-de-arquivos-de-teste-testsgens_storyc)); ele permite especificar **tamanho por MiB**, **seed** e **palavra‑chave**.

---

## 10. Guia de Defesa: Domínio
- **Explique o porquê**: blocos independentes → **seek seletivo**; índice compacto → **cabe em RAM**; KMP incremental → **encontra fronteiras**.  
- **Demonstre medição**: Working Set ~ bloco; contagens E2×E3 iguais.  
- **Trade‑offs**: bloco maior melhora compressão mas aumenta RAM/latência; bloco menor faz o inverso.

---

## 11. Estrutura do Repositório
```
.
├─ include/
│  ├─ cli.h         # Flags/opts e interface CLI
│  ├─ index.h       # Header/índice do .ed2c (structs e I/O)
│  ├─ kmp.h         # Assinaturas do KMP
│  ├─ lzw.h         # Assinaturas do LZW e comandos (E1/E3/E4)
│  ├─ meuprog.h     # Constantes globais, nomes, versão
│  └─ types.h       # Tipos fixos, buffers padrão, utils
├─ src/
│  ├─ main.c        # Parse de argumentos, roteamento e menu interativo
│  ├─ cli.c         # UI, relatório, manipulação de flags e saída
│  ├─ kmp.c         # KMP em streaming (Etapa 2) e utilidades
│  ├─ index.c       # Leitura/escrita de header e índice .ed2c
│  ├─ lzw.c         # LZW por blocos + Etapas 1/3/4
│  └─ sysmon.c      # Medição de tempo/memória (Windows PSAPI)
└─ tests/
   ├─ gen_story.c   # Gerador de textos grandes para teste
   └─ samples/      # Exemplos de entradas/saídas (opcional)
```

---

## 12. Licença e Créditos
Projeto acadêmico para **ED‑II**. Uso educacional.  
Agradecimentos ao professor e à equipe. Métricas de memória via **PSAPI** (Windows).

---

> **Contato/observação final:** offsets reportados são sempre do arquivo **original**; para auditoria em massa, prefira `--out=matches.txt` e rode com `--silent`.
