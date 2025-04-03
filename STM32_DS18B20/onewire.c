#include "config.h"
#include "onewire.h"

//
// Fonction de délai pour les timings constants du bus 1-Wire
//
void OneWire_Delay(OneWire_t* OneWireStruct, uint16_t us) {
    uint32_t start = __HAL_TIM_GET_COUNTER(OneWireStruct->Timer); // Récupère la valeur initiale du compteur du timer
    while ((__HAL_TIM_GET_COUNTER(OneWireStruct->Timer) - start) < us); // Attend jusqu'à ce que le délai soit écoulé
}

//
// Contrôle de la direction du bus
//
void OneWire_BusInputDirection(OneWire_t *onewire) {
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT; // Configure la broche en entrée
    GPIO_InitStruct.Pull = GPIO_NOPULL; // Pas de pull-up ou pull-down (résistance externe utilisée)
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM; // Fréquence moyenne pour le GPIO
    GPIO_InitStruct.Pin = onewire->GPIO_Pin; // Configure la broche utilisée pour le bus 1-Wire
    HAL_GPIO_Init(onewire->GPIOx, &GPIO_InitStruct); // Réinitialise la broche avec les nouveaux paramètres
}

void OneWire_BusOutputDirection(OneWire_t *onewire) {
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD; // Configure la broche en sortie open-drain
    GPIO_InitStruct.Pull = GPIO_NOPULL; // Pas de pull-up ou pull-down (résistance externe utilisée)
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM; // Fréquence moyenne pour le GPIO
    GPIO_InitStruct.Pin = onewire->GPIO_Pin; // Configure la broche utilisée pour le bus 1-Wire
    HAL_GPIO_Init(onewire->GPIOx, &GPIO_InitStruct); // Réinitialise la broche avec les nouveaux paramètres
}

//
// Contrôle de l'état de sortie de la broche du bus
//
void OneWire_OutputLow(OneWire_t *onewire) {
    onewire->GPIOx->BSRR = onewire->GPIO_Pin << 16; // Met la broche à l'état bas
}

void OneWire_OutputHigh(OneWire_t *onewire) {
    onewire->GPIOx->BSRR = onewire->GPIO_Pin; // Met la broche à l'état haut
}

//
// Signal de réinitialisation du bus 1-Wire
//
// Retourne :
// 0 - Réinitialisation réussie
// 1 - Erreur
//
uint8_t OneWire_Reset(OneWire_t* OneWireStruct) {
    uint8_t i;

    OneWire_OutputLow(OneWireStruct); // Met la broche à l'état bas
    OneWire_BusOutputDirection(OneWireStruct); // Configure la broche en sortie
    OneWire_Delay(OneWireStruct, 480); // Attend 480 µs pour le signal de réinitialisation

    OneWire_BusInputDirection(OneWireStruct); // Relâche le bus en configurant la broche en entrée
    OneWire_Delay(OneWireStruct, 70); // Attend 70 µs pour permettre au périphérique de répondre

    i = HAL_GPIO_ReadPin(OneWireStruct->GPIOx, OneWireStruct->GPIO_Pin); // Vérifie si le bus est à l'état bas
    OneWire_Delay(OneWireStruct, 410); // Attend 410 µs pour terminer la réinitialisation

    return i; // Retourne 0 si la réinitialisation est réussie, 1 sinon
}

//
// Opérations d'écriture/lecture
//
void OneWire_WriteBit(OneWire_t* OneWireStruct, uint8_t bit) {
    if (bit) { // Envoie un '1'
        OneWire_OutputLow(OneWireStruct); // Met le bus à l'état bas
        OneWire_BusOutputDirection(OneWireStruct); // Configure la broche en sortie
        OneWire_Delay(OneWireStruct, 6); // Attend 6 µs

        OneWire_BusInputDirection(OneWireStruct); // Relâche le bus (état haut via pull-up)
        OneWire_Delay(OneWireStruct, 64); // Attend 64 µs
    } else { // Envoie un '0'
        OneWire_OutputLow(OneWireStruct); // Met le bus à l'état bas
        OneWire_BusOutputDirection(OneWireStruct); // Configure la broche en sortie
        OneWire_Delay(OneWireStruct, 60); // Attend 60 µs

        OneWire_BusInputDirection(OneWireStruct); // Relâche le bus (état haut via pull-up)
        OneWire_Delay(OneWireStruct, 10); // Attend 10 µs
    }
}

uint8_t OneWire_ReadBit(OneWire_t* OneWireStruct) {
    uint8_t bit = 0; // Par défaut, le bit lu est 0

    OneWire_OutputLow(OneWireStruct); // Met le bus à l'état bas pour initier la lecture
    OneWire_BusOutputDirection(OneWireStruct); // Configure la broche en sortie
    OneWire_Delay(OneWireStruct, 2); // Attend 2 µs

    OneWire_BusInputDirection(OneWireStruct); // Relâche le bus pour permettre au périphérique de répondre
    OneWire_Delay(OneWireStruct, 10); // Attend 10 µs pour lire le bit

    if (HAL_GPIO_ReadPin(OneWireStruct->GPIOx, OneWireStruct->GPIO_Pin)) { // Lit l'état du bus
        bit = 1; // Si le bus est haut, le bit lu est 1
    }

    OneWire_Delay(OneWireStruct, 50); // Attend 50 µs pour terminer la lecture

    return bit; // Retourne le bit lu
}

void OneWire_WriteByte(OneWire_t* onewire, uint8_t byte) {
    uint8_t i = 8;

    do {
        OneWire_WriteBit(onewire, byte & 1); // Envoie le bit de poids faible en premier
        byte >>= 1; // Décale les bits vers la droite
    } while (--i);
}

uint8_t OneWire_ReadByte(OneWire_t* onewire) {
    uint8_t i = 8, byte = 0;

    do {
        byte >>= 1; // Décale les bits vers la droite
        byte |= (OneWire_ReadBit(onewire) << 7); // Lit le bit et le place au bon endroit
    } while (--i);

    return byte; // Retourne l'octet lu
}

//
// 1-Wire search operations
//
void OneWire_ResetSearch(OneWire_t* onewire)
{
	// Clear the search results
	onewire->LastDiscrepancy = 0;
	onewire->LastDeviceFlag = 0;
	onewire->LastFamilyDiscrepancy = 0;
}

uint8_t OneWire_Search(OneWire_t* onewire, uint8_t command)
{
	uint8_t id_bit_number;
	uint8_t last_zero, rom_byte_number, search_result;
	uint8_t id_bit, cmp_id_bit;
	uint8_t rom_byte_mask, search_direction;

	id_bit_number = 1;
	last_zero = 0;
	rom_byte_number = 0;
	rom_byte_mask = 1;
	search_result = 0;

	if (!onewire->LastDeviceFlag) // If last device flag is not set
	{
		if (OneWire_Reset(onewire)) // Reset bus
		{
			// If error while reset - reset search results
			onewire->LastDiscrepancy = 0;
			onewire->LastDeviceFlag = 0;
			onewire->LastFamilyDiscrepancy = 0;
			return 0;
		}

		OneWire_WriteByte(onewire, command); // Send searching command

		// Searching loop, Maxim APPLICATION NOTE 187
		do
		{
			id_bit = OneWire_ReadBit(onewire); // Read a bit 1
			cmp_id_bit = OneWire_ReadBit(onewire); // Read the complement of bit 1

			if ((id_bit == 1) && (cmp_id_bit == 1)) // 11 - data error
			{
				break;
			}
			else
			{
				if (id_bit != cmp_id_bit)
				{
					search_direction = id_bit;  // Bit write value for search
				}
				else // 00 - 2 devices
				{
					// Table 3. Search Path Direction
					if (id_bit_number < onewire->LastDiscrepancy)
					{
						search_direction = ((onewire->ROM_NO[rom_byte_number] & rom_byte_mask) > 0);
					}
					else
					{
						// If bit is equal to last - pick 1
						// If not - then pick 0
						search_direction = (id_bit_number == onewire->LastDiscrepancy);
					}

					if (search_direction == 0) // If 0 was picked, write it to LastZero
					{
						last_zero = id_bit_number;

						if (last_zero < 9) // Check for last discrepancy in family
						{
							onewire->LastFamilyDiscrepancy = last_zero;
						}
					}
				}

				if (search_direction == 1)
				{
					onewire->ROM_NO[rom_byte_number] |= rom_byte_mask; // Set the bit in the ROM byte rom_byte_number
				}
				else
				{
					onewire->ROM_NO[rom_byte_number] &= ~rom_byte_mask; // Clear the bit in the ROM byte rom_byte_number
				}
				
				OneWire_WriteBit(onewire, search_direction); // Search direction write bit

				id_bit_number++; // Next bit search - increase the id
				rom_byte_mask <<= 1; // Shoft the mask for next bit

				if (rom_byte_mask == 0) // If the mask is 0, it says the whole byte is read
				{
					rom_byte_number++; // Next byte number
					rom_byte_mask = 1; // Reset the mask - first bit
				}
			}
		} while(rom_byte_number < 8);  // Read 8 bytes

		if (!(id_bit_number < 65)) // If all read bits number is below 65 (8 bytes)
		{
			onewire->LastDiscrepancy = last_zero;

			if (onewire->LastDiscrepancy == 0) // If last discrepancy is 0 - last device found
			{
				onewire->LastDeviceFlag = 1; // Set the flag
			}

			search_result = 1; // Searching successful
		}
	}

	// If no device is found - reset search data and return 0
	if (!search_result || !onewire->ROM_NO[0])
	{
		onewire->LastDiscrepancy = 0;
		onewire->LastDeviceFlag = 0;
		onewire->LastFamilyDiscrepancy = 0;
		search_result = 0;
	}

	return search_result;
}

//
//	Return first device on 1-Wire bus
//
uint8_t OneWire_First(OneWire_t* onewire)
{
	OneWire_ResetSearch(onewire);

	return OneWire_Search(onewire, ONEWIRE_CMD_SEARCHROM);
}

//
//	Return next device on 1-Wire bus
//
uint8_t OneWire_Next(OneWire_t* onewire)
{
   /* Leave the search state alone */
   return OneWire_Search(onewire, ONEWIRE_CMD_SEARCHROM);
}

//
//	Select a device on bus by address
//
void OneWire_Select(OneWire_t* onewire, uint8_t* addr)
{
	uint8_t i;
	OneWire_WriteByte(onewire, ONEWIRE_CMD_MATCHROM); // Match ROM command
	
	for (i = 0; i < 8; i++)
	{
		OneWire_WriteByte(onewire, *(addr + i));
	}
}

//
//	Select a device on bus by pointer to ROM address
//
void OneWire_SelectWithPointer(OneWire_t* onewire, uint8_t *ROM)
{
	uint8_t i;
	OneWire_WriteByte(onewire, ONEWIRE_CMD_MATCHROM); // Match ROM command
	
	for (i = 0; i < 8; i++)
	{
		OneWire_WriteByte(onewire, *(ROM + i));
	}	
}

//
//	Get the ROM of found device
//
void OneWire_GetFullROM(OneWire_t* onewire, uint8_t *firstIndex)
{
	uint8_t i;
	for (i = 0; i < 8; i++) {
		*(firstIndex + i) = onewire->ROM_NO[i];
	}
}

//
// Calcul du CRC
//
uint8_t OneWire_CRC8(uint8_t *addr, uint8_t len) {
    uint8_t crc = 0, inbyte, i, mix;

    while (len--) {
        inbyte = *addr++; // Récupère l'octet suivant
        for (i = 8; i; i--) { // Parcourt les 8 bits de l'octet
            mix = (crc ^ inbyte) & 0x01; // Calcule le bit de parité
            crc >>= 1; // Décale le CRC vers la droite
            if (mix) {
                crc ^= 0x8C; // Applique le polynôme si nécessaire
            }
            inbyte >>= 1; // Décale l'octet vers la droite
        }
    }

    return crc; // Retourne le CRC calculé
}

//
//	1-Wire initialization
//
void OneWire_Init(OneWire_t* OneWireStruct) {
    OneWireStruct->GPIOx = ONEWIRE_GPIO_PORT;
    OneWireStruct->GPIO_Pin = ONEWIRE_GPIO_PIN;
    OneWireStruct->Timer = ONEWIRE_TIMER;

    HAL_TIM_Base_Start(ONEWIRE_TIMER);
}

