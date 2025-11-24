# Define o compilador que será usado.
# Em sistemas Linux/Unix, "cc" normalmente aponta para gcc ou clang.
CC = cc

# Flags que ativam avisos importantes do compilador.
# -Wall    -> mostra avisos padrão
# -Wextra  -> mostra avisos adicionais
# -pedantic -> exige conformidade estrita ao padrão C
FLAGS = -Wall -Wextra -pedantic

# Opção de otimização.
# -O3 aplica otimizações pesadas para melhorar performance.
OPTIONS = -O3

# Nome do executável final que será gerado.
EXE = minecweeper

# Nome do arquivo-fonte principal (sem .c)
SRC = main

# Alvo padrão executado quando você digita apenas "make".
# Aqui ele chama o alvo $(EXE).
all: $(EXE)

# Regra para compilar o programa.
# O executável depende de "main.c".
# Se main.c mudar, o make recompila o programa.
$(EXE): $(SRC).c
	$(CC) $(OPTIONS) $(FLAGS) -o $@ $<

# Marca o alvo "clean" como um alvo que não representa arquivos reais.
.PHONY: clean

# Comando para limpar os arquivos gerados.
# Remove o executável com detalhes (-v)
clean:
	rm -frv $(EXE)