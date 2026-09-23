# Como os experimentos deste relatório foram executados

Compilação:
```bash
gcc -Wall -Wextra -O2 -o inseridor inseridor.c
```

## Experimento A — uma única busca/ordenação, variando n0
(pasta `experimentos/sweepA_unico_por_n0/`)

Para cada n0 em {250, 500, 1000, 2000, 4000}, o destino foi reiniciado para
conter exatamente os n0 primeiros registros de `novas_entradas.csv`
(`destino_base_<n0>.csv`), e uma única entrada nova e inédita
(`entrada_unica_<n0>.csv`) foi processada:

```bash
for n0 in 250 500 1000 2000 4000; do
  cp experimentos/sweepA_unico_por_n0/destino_base_${n0}.csv destino_run.csv
  ./inseridor experimentos/sweepA_unico_por_n0/entrada_unica_${n0}.csv destino_run.csv
done
```

Isso isola o custo de uma única busca sequencial, uma única busca binária e
uma única ordenação completa (mergesort), em função pura de n0.

## Experimento B — lote de m novas entradas, n0 = 4000 fixo
(pasta `experimentos/sweepB_lote_m_variavel/`)

O destino foi fixado em n0 = 4000 (`destino_base_4000.csv`), restaurado antes
de cada execução, e processado contra lotes de tamanho m em
{10, 20, 40, 80, 160} (`sublote_<m>.csv`), extraídos de uma faixa de 328
registros nunca usada nos primeiros 4000, garantindo que nenhuma entrada
fosse redundante:

```bash
for m in 10 20 40 80 160; do
  cp experimentos/sweepB_lote_m_variavel/destino_base_4000.csv destino_run.csv
  ./inseridor experimentos/sweepB_lote_m_variavel/sublote_${m}.csv destino_run.csv
done
```

## Experimento C — escala grande, destino inicial vazio

Reaproveita a execução original do grupo: `novas_entradas.csv` completo
(4.328 registros) processado contra um `destino.csv` inicialmente vazio.
Resultado em `estatisticas_exemplo_completo.csv` (linha com
`n_inicial_destino=0`, `m_novas_entradas=4328`).

Em todos os casos, os resultados de cada execução são lidos do arquivo
`estatisticas_algoritmos.csv` gerado pelo próprio programa (modo append).
