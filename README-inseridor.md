# Inseridor de Instituições de Ensino Superior (e-MEC) — Análise de Complexidade

Programa em C que insere novas instituições de ensino superior em um
arquivo CSV de destino, garantindo que a coluna `CODIGO_DA_IES`
permaneça sem valores duplicados, e instrumenta a execução com **seis**
implementações de algoritmos clássicos para permitir a validação
experimental de suas complexidades de tempo e espaço.

---

## 1. O dataset

### 1.1 Origem

- **Arquivo:** `novas_entradas.csv` (nome dado ao arquivo original neste
  trabalho)
- **Nome original:** `PDA_Lista_Instituicoes_Ensino_Superior_do_Brasil_EMEC.csv`
- **Fonte:** Portal de Dados Abertos do MEC
- **URL de download:**
  ```
  https://dadosabertos.mec.gov.br/images/conteudo/Ind-ensino-superior/2022/PDA_Lista_Instituicoes_Ensino_Superior_do_Brasil_EMEC.csv
  ```
- **Conteúdo:** Cadastro e-MEC das Instituições de Educação Superior do
  Brasil — uma linha por instituição (ativa ou extinta).
- **Tamanho:** 4.328 registros de dados (+ 1 linha de cabeçalho).
- **Codificação:** UTF-8. Quebra de linha estilo Windows (`\r\n`).
- **Delimitador de campo:** vírgula (`,`).

### 1.2 Estrutura das colunas

```
CODIGO_DA_IES,NOME_DA_IES,SIGLA,CATEGORIA_DA_IES,COMUNITARIA,
CONFESSIONAL,FILANTROPICA,ORGANIZACAO_ACADEMICA,
CODIGO_MUNICIPIO_IBGE,MUNICIPIO,UF,SITUACAO_IES
```

Exemplo de linha real:
```
21995,Faculdade de Tecnologia Senac Curitiba,,Privada,N,N,N,Faculdade,000000004106902,Curitiba,PR,Ativa
```

Algumas linhas trazem o campo `NOME_DA_IES` entre aspas, pois o nome
contém vírgulas internas — por exemplo:
```
725,"FACULDADE ESTADUAL DE FILOSOFIA, CIÊNCIAS E LETRAS DE PARANAGUÁ",FAFIPAR,Pública,N,N,N,Faculdade,null,null,null,Extinta
```

### 1.3 Por que este dataset atende aos requisitos do enunciado

O trabalho exige um arquivo CSV em que (a) alguma coluna não permita
valores duplicados por regra de negócio, e (b) os valores dessa coluna
não estejam ordenados no arquivo. A coluna `CODIGO_DA_IES` foi
escolhida como chave, e ambas as condições foram **verificadas
empiricamente** antes da escolha (não apenas presumidas):

| Verificação | Resultado |
|---|---|
| Total de linhas de dados | 4.328 |
| Valores distintos de `CODIGO_DA_IES` | 4.328 (nenhuma duplicata) |
| Primeiros códigos na ordem original do arquivo | `21995, 1768, 5701, 23261, 4250, ...` |
| Primeiros códigos após ordenação numérica | `1, 2, 3, 4, 5, ...` |

A comparação entre a terceira e a quarta linha da tabela mostra que a
ordem original do arquivo **não é a ordem numérica crescente** — os
valores estão de fato embaralhados, satisfazendo o segundo requisito.

Vale registrar que a escolha deste dataset veio depois de duas
tentativas descartadas: arquivos de "Bolsistas UAB" da CAPES, que se
revelaram **logs de pagamento** (a mesma pessoa, identificada pelo
CPF, aparece repetidas vezes — uma vez por mês em que recebeu bolsa),
e não cadastros com chave única. O e-MEC, por ser um cadastro de
instituições (uma linha por instituição), tem a natureza estrutural
correta para o exercício.

### 1.4 `CODIGO_DA_IES` é seguro de extrair mesmo com nomes citados

Como `CODIGO_DA_IES` é **sempre o primeiro campo da linha, sempre
puramente numérico e nunca está entre aspas**, ele pode ser extraído
com segurança mesmo quando outros campos da mesma linha (como
`NOME_DA_IES`) contêm vírgulas internas entre aspas. O programa usa
`strtol`, que lê os dígitos iniciais da linha e para automaticamente
ao encontrar a primeira vírgula — não é necessário um parser CSV
completo (com suporte a aspas) para isso.

---

## 2. Compilação e execução

```bash
gcc -Wall -Wextra -O2 -o inseridor inseridor.c
```

```bash
./inseridor <novas_entradas.csv> <destino.csv>
```

| Argumento | Significado |
|---|---|
| `novas_entradas.csv` | Arquivo CSV com as novas instituições a inserir |
| `destino.csv` | Arquivo CSV de cadastro consolidado, sem duplicatas, sempre não ordenado |

Exemplo:
```bash
./inseridor novas_entradas.csv destino.csv
```

Diferente do trabalho anterior (CAPES), o enunciado desta atividade
especifica **apenas dois** argumentos de linha de comando. Por isso, o
nome do arquivo de estatísticas não é recebido via `argv` — ele é fixo:
`estatisticas_algoritmos.csv`, criado no diretório de execução. Esse
arquivo é aberto em modo `append`: cada execução do programa acrescenta
uma nova linha, permitindo acumular uma tabela de experimentos ao
rodar o programa várias vezes com lotes de tamanhos diferentes, sem
precisar de um terceiro argumento.

---

## 3. Como o programa funciona

O programa implementa duas soluções distintas para o mesmo problema
(evitar duplicidade de `CODIGO_DA_IES`), rodando **ambas ao mesmo
tempo** sobre os mesmos dados, para permitir comparação direta.

### 3.1 Carregamento inicial

O destino (se já existir) é lido inteiro para dentro de um
`UnsortedRegistry`: um par de vetores dinâmicos — um de códigos
(`long`) e um de linhas completas (`char *`) — que preserva **a ordem
original do arquivo**. Esse é o vetor não ordenado usado pela Solução 1.

Em seguida, uma cópia dos códigos desse estado inicial (chamado de
`n0`, o tamanho do destino *antes* do lote atual) é ordenada com
**mergesort**, gerando o vetor ordenado em RAM que serve de base para
a Solução 2. Essa ordenação é feita **duas vezes, de forma
independente** — uma com a versão iterativa do mergesort, outra com a
recursiva — apenas para poder comparar suas métricas.

### 3.2 Solução 1 — busca sequencial (vetor não ordenado)

Para cada nova entrada lida de `novas_entradas.csv`:

- `seq_search_iterative`: percorre o vetor não ordenado do início ao
  fim, comparando cada elemento com `strcmp` até encontrar o valor ou
  chegar ao fim. **Esta é a implementação que efetivamente decide** se
  a entrada é aceita ou rejeitada.
- `seq_search_recursive`: mesma lógica, mas expressa como uma função
  recursiva (uma chamada por elemento comparado). Roda em paralelo,
  apenas para fins de instrumentação — nunca influencia o resultado
  gravado no arquivo.

Se o valor não for encontrado, a linha completa é copiada para o final
do arquivo de destino (que **permanece não ordenado**, como o
enunciado exige), e o código passa a integrar o vetor não ordenado
para as buscas seguintes do mesmo lote.

Se o valor já existir, é impressa a mensagem:
```
ERRO: codigo de IES redundante encontrado: <codigo>
```
e a execução continua normalmente, sem interrupção.

### 3.3 Solução 2 — mergesort + busca binária (vetor ordenado em RAM)

Para a mesma nova entrada, em paralelo:

- `bin_search_iterative` e `bin_search_recursive` procuram o código no
  vetor ordenado. Diferente da busca sequencial, elas não decidem se a
  entrada é aceita — servem apenas para instrumentação e para
  determinar a posição de inserção correta caso a entrada seja aceita
  (decisão essa que vem da Solução 1).
- Se a entrada for aceita, `sorted_insert_at` insere o novo código na
  posição correta do vetor ordenado, **deslocando todos os elementos
  seguintes uma posição à direita** para preservar a ordenação. Esse
  deslocamento é contado separadamente (`sorted_insert_moves`), pois é
  o verdadeiro custo estrutural de manter um vetor ordenado.

Note que o vetor ordenado existe **apenas na memória**, para viabilizar
a busca binária — o arquivo `destino.csv` em disco nunca é reescrito em
ordem; novas entradas são sempre anexadas ao final, como exigido.

### 3.4 Geração das estatísticas

Ao final da execução, uma linha é anexada a
`estatisticas_algoritmos.csv` com:

```
n_inicial_destino,m_novas_entradas,nomes_inseridos,nomes_redundantes,
seqiter_comparacoes,seqrec_comparacoes,
msiter_comparacoes,msiter_movimentacoes,msrec_comparacoes,msrec_movimentacoes,
biniter_comparacoes,binrec_comparacoes,
insercao_ordenada_movimentacoes
```

O cabeçalho só é escrito na primeira vez que o arquivo é criado;
execuções seguintes apenas acrescentam linhas, permitindo montar a
tabela de experimentos automaticamente ao rodar o programa várias
vezes com lotes de tamanhos diferentes.

---

## 4. Pontos mais importantes para a análise

### 4.1 As versões iterativa e recursiva de cada algoritmo produzem o
mesmo número de comparações (quando o algoritmo é o mesmo)

Isso **não é coincidência, é esperado e deve ser validado
empiricamente**: busca sequencial iterativa e recursiva executam
exatamente as mesmas comparações, na mesma ordem — a única diferença é
*como* a iteração é expressa (laço `for` vs. pilha de chamadas). O
mesmo vale para busca binária. Já o mergesort iterativo (bottom-up) e
o recursivo (top-down) podem apresentar pequenas diferenças no número
de comparações quando `n` não é potência de 2, porque particionam os
subvetores de formas ligeiramente diferentes — mas essa diferença é
marginal e não afeta a classe de complexidade.

**O que muda entre as versões iterativa e recursiva não é o tempo, é o
espaço**: a versão recursiva paga uma pilha de chamadas adicional (O(n)
para busca sequencial, O(log n) para mergesort e busca binária),
enquanto a versão iterativa usa espaço extra O(1). Essa é a
comparação central pedida no enunciado para cada par de algoritmos.

### 4.2 O paradoxo central da Solução 2: a busca é rápida, mas manter o vetor ordenado é caro

Este é o ponto mais importante da análise comparativa entre as duas
soluções. Nos testes realizados (328 novas entradas contra um destino
de 4.000 registros):

| Métrica | Valor observado |
|---|---|
| Comparações — busca sequencial (Solução 1) | 1.365.628 |
| Comparações — busca binária (Solução 2) | 7.914 |
| Movimentações — inserção no vetor ordenado (Solução 2) | 691.960 |

A busca binária é, individualmente, ordens de magnitude mais barata que
a sequencial (O(log n) contra O(n) por busca). **Mas** cada entrada
aceita exige deslocar, em média, metade dos elementos do vetor ordenado
para preservar a ordem — um custo O(n) por inserção, que **anula
grande parte da vantagem teórica da busca binária** quando o volume de
inserções é comparável ao tamanho do vetor.

Isso significa que a resposta correta para "Compare a complexidade
temporal e espacial da solução 2 e da solução 1" **não é simplesmente
"Solução 2 é sempre melhor porque log n < n"** — a resposta depende de
quantas inserções ocorrem em relação ao tamanho do vetor, e deve ser
discutida com esses dados empíricos, não apenas com a comparação
assintótica isolada de busca binária vs. busca sequencial.

### 4.3 Duplicidade também é verificada dentro do próprio lote

O vetor não ordenado (Solução 1) e o vetor ordenado (Solução 2)
**crescem progressivamente durante o processamento do mesmo lote**: uma
entrada aceita passa a fazer parte da base de comparação para as
entradas seguintes do mesmo arquivo de origem. Isso significa que, se
`novas_entradas.csv` tivesse dois valores iguais entre si (não apenas
repetidos em relação ao destino original), o segundo seria corretamente
rejeitado como redundante. Essa é uma decisão de projeto explícita, não
detalhada literalmente pelo enunciado, e deve ser documentada no
relatório.

### 4.4 Verificações de sanidade (auto-consistência)

O código inclui duas checagens automáticas que emitem avisos em
`stderr` caso as implementações divirjam entre si:

- Se `seq_search_iterative` e `seq_search_recursive` discordarem sobre
  se um código existe ou não (o que indicaria um bug em uma das duas
  implementações).
- Se `bin_search_iterative` e `bin_search_recursive` discordarem da
  mesma forma.

Nos testes realizados, nenhum desses avisos foi disparado — as
implementações concordam integralmente entre si, o que é evidência de
corretude.

### 4.5 Profundidade de pilha das versões recursivas

Com o tamanho deste dataset (pouco mais de 4.300 registros), a
profundidade de pilha das versões recursivas está longe de qualquer
risco de estouro:

- Busca sequencial recursiva: profundidade O(n) — até ~4.700 chamadas
  aninhadas no pior caso, seguro para o tamanho da pilha padrão (~8 MB
  no Linux).
- Mergesort recursivo e busca binária recursiva: profundidade O(log n)
  — no máximo ~13 chamadas aninhadas, sem risco algum.

Essa observação deve constar no relatório como parte da discussão de
espaço, mas vale registrar que, com datasets muito maiores (na ordem de
milhões de registros), a busca sequencial recursiva especificamente
poderia se tornar um risco real de *stack overflow* — o que reforça, na
prática, a diferença de complexidade de espaço O(n) vs. O(1) entre as
versões recursiva e iterativa da busca sequencial.

### 4.6 Preservação da integridade do CSV

Nomes de instituição contendo vírgulas internas (delimitadas por aspas)
foram testados e preservados corretamente na cópia, porque o programa
nunca reconstrói a linha campo a campo — ele extrai apenas o primeiro
campo (`CODIGO_DA_IES`, sempre numérico e nunca citado) para fins de
comparação, e copia a linha inteira, sem modificação, para o arquivo de
destino.

---

## 5. Roteiro sugerido para os experimentos

1. Separe o dataset em um destino inicial (por exemplo, os primeiros
   4.000 registros) e um arquivo de novas entradas com os registros
   restantes.
2. Para gerar pontos de dados com `n` crescente, extraia sublotes de
   tamanhos diferentes (ex: 10, 20, 40, 80, 160) do arquivo de novas
   entradas.
3. Para cada sublote, restaure o destino ao seu estado inicial e execute:
   ```bash
   ./inseridor sublote_n.csv destino.csv
   ```
   sempre apontando para o mesmo `estatisticas_algoritmos.csv`.
4. Ao final, a tabela acumulada em `estatisticas_algoritmos.csv` terá
   uma linha por experimento, pronta para comparar contra os modelos
   teóricos de complexidade de cada um dos seis algoritmos.

---

## 6. O programa é genérico para outro CSV?

Uma análise do código, feita a pedido, avaliou se `inseridor.c`, do
jeito que está, funcionaria com qualquer outro arquivo CSV que atenda
aos requisitos do enunciado (coluna sem duplicatas, valores não
ordenados), sem alterações. A resposta é **parcialmente**: o programa
funciona de forma genérica apenas sob duas condições restritivas, e há
um ponto que falha silenciosamente fora delas.

### 6.1 Cabeçalho hardcoded (falha real, se o destino ainda não existir)

```c
if (!dest_exists) {
    strcpy(header_line,
           "CODIGO_DA_IES,NOME_DA_IES,SIGLA,CATEGORIA_DA_IES,COMUNITARIA,"
           "CONFESSIONAL,FILANTROPICA,ORGANIZACAO_ACADEMICA,"
           "CODIGO_MUNICIPIO_IBGE,MUNICIPIO,UF,SITUACAO_IES");
}
```

Se o arquivo de destino **ainda não existir** na primeira execução, o
programa grava esse cabeçalho literal do e-MEC, independentemente do
dataset realmente usado — um destino criado do zero para outro CSV
sairia com colunas erradas. Se o destino **já existir** (por exemplo,
criado manualmente antes da primeira execução, com o cabeçalho correto
do novo dataset), esse trecho nunca roda — o cabeçalho real é lido do
arquivo, e o problema desaparece. É contornável, mas exige uma etapa
manual fora do fluxo normal do programa.

### 6.2 Extração da chave assume "primeiro campo, puramente numérico"

```c
static long extract_code(const char *line) {
    return strtol(line, NULL, 10);
}
```

`strtol` lê apenas os dígitos no início da linha, até o primeiro
caractere não numérico (o delimitador). Isso só funciona
genericamente se a coluna-chave for **a primeira coluna** e for
**puramente numérica**. Fora disso, o risco não é apenas teórico:

- Se a chave estivesse em outra posição, `extract_code` extrairia
  sempre a coluna errada, sem nenhum erro ou aviso.
- Se a chave fosse alfanumérica (por exemplo, um CPF mascarado como
  `***.939.357-**`, como no dataset de Bolsistas UAB descartado antes
  de se chegar ao e-MEC), `strtol` pararia no primeiro caractere não
  numérico e retornaria **0** para todas as linhas — fazendo o
  programa tratar tudo como duplicado a partir da segunda entrada,
  sem nenhuma mensagem explicando o motivo.

### 6.3 O delimitador, por outro lado, já é genérico

Como `extract_code` lê apenas os dígitos iniciais da linha (sem
depender de localizar uma vírgula especificamente), o programa
funcionaria igualmente bem com `;` como delimitador, desde que a
chave continue sendo o primeiro campo e puramente numérica. O
delimitador em si não é uma limitação.

### 6.4 Resumo

| Aspecto | Genérico? |
|---|---|
| Delimitador do CSV | Sim — qualquer um funciona |
| Cabeçalho quando destino já existe | Sim — lido dinamicamente do arquivo |
| Cabeçalho quando destino não existe | Não — grava o cabeçalho fixo do e-MEC |
| Posição da coluna-chave | Não — assume que é sempre a primeira coluna |
| Tipo da coluna-chave | Não — assume que é sempre numérica inteira (`long`) |
| `MAX_LINE 4096` | Limitação de buffer genérica, não específica do e-MEC |

**Conclusão**: o programa, como está, só funcionaria sem alterações em
outro dataset se a chave sem duplicatas for a primeira coluna, for
puramente numérica, e o arquivo de destino já existir com o cabeçalho
correto antes da primeira execução. Fora dessas condições, o código
precisaria de ajustes — não é uma solução genérica de "plug-and-play"
para qualquer CSV que atenda aos critérios do enunciado, apesar de o
enunciado pedir "um programa que insere novas entradas... no arquivo
csv escolhido" de forma mais abstrata. Essa limitação foi identificada
deliberadamente e mantida sem correção, por decisão registrada nesta
seção — o programa permanece acoplado à estrutura específica do
dataset e-MEC.
