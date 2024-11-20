include config.mk

SRC = ./src

$(BIN):
	@cd $(SRC) && make $@

clean:
	@cd $(SRC) && make $@