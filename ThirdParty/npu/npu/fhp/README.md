# Financial Simulation - Chart of Accounts as Virtual Hardware

This example demonstrates modeling a financial Chart of Accounts (CoA) as virtual hardware architecture, leveraging llama.cpp to simulate, route, and reason over financial operations.

## Concept

- **Accounts as Hardware Nodes**: Each account (Assets, Liabilities, Income, Expenses, Equity) is treated as a hardware node or pin
- **Transactions as Signal Routing**: Money transfers are modeled as electrical signals flowing through circuits
- **Account Groups as Subsystems**: Account classifications map to hardware modules/sub-chips
- **Business Rules as Circuit Logic**: Double-entry accounting becomes conservation laws, validations become electrical laws

## Usage

```bash
# Demo mode (no LLM required) - shows complete financial circuit simulation
./llama-financial-sim --demo

# Interactive mode without LLM - explore the financial circuit
./llama-financial-sim --interactive

# Full LLM-powered analysis mode
./llama-financial-sim -m model.gguf --interactive

# Load custom chart of accounts
./llama-financial-sim -m model.gguf --coa accounts.json

# Trace specific transaction flows
./llama-financial-sim -m model.gguf --trace "Transfer $1000 from Cash to Equipment"
```

## Interactive Commands

- `balance-sheet` - Show current circuit state (all account balances)
- `income-statement` - Show signal flow analysis (revenue vs expenses)  
- `transaction` - Process a new signal routing operation
- `quick-demo` - Run quick demo transactions
- `analyze [query]` - Analyze circuit using LLM (requires model)
- `accounts` - List all account nodes and their hardware descriptions
- `reset` - Reset circuit to initial state
- `quit` - Exit simulator

## Hardware Analogy Mapping

### Account Types as Hardware Subsystems
- **Assets** → Input/Storage Registers (Cash, Equipment, Inventory)
- **Liabilities** → Output Buffers/Obligations (Loans, Accounts Payable) 
- **Equity** → Core Processing Unit (Owner's Equity, Retained Earnings)
- **Revenue** → Signal Generators/Sources (Sales, Service Revenue)
- **Expenses** → Signal Consumers/Sinks (Salaries, Rent, Utilities)

### Signal Polarity
- **Positive Signal (Debit)**: Increases Assets and Expenses
- **Negative Signal (Credit)**: Increases Liabilities, Equity, and Revenue

### Conservation Laws
- **Double-Entry Accounting** = Conservation of Charge/Signal
- Every transaction must have equal debits and credits
- Total circuit "energy" is conserved across all operations

## Example Output

```
=== FINANCIAL CIRCUIT DEMO SCENARIO ===
Simulating a small business financial circuit...

2. POWER INJECTION - Owner Investment:
✓ Signal routing completed - $50,000 injected into circuit

Node: 1101 (Cash)
  Subsystem: Asset (Storage/Input)  
  Signal Flow: +50000 (Debit/Positive)
  Current State: $50000.00

=== HARDWARE CIRCUIT PERFORMANCE METRICS ===
Total Signal Injections: $78,000 (Investment + Revenue + Loan)
Total Signal Consumption: $19,200 (Equipment + Expenses) 
Net Circuit Gain: $58,800
Circuit Efficiency: 75.4%
```

## LLM Prompt Templates

When used with a model, the system generates hardware-focused prompts:

- "Analyze this financial circuit using hardware engineering principles"
- "Trace the propagation of this transaction through the financial circuit"
- "Validate circuit integrity and identify potential signal routing issues"
- "Explain signal flows, node interactions, and subsystem behavior"

## Features

- ✅ Chart of Accounts hierarchy modeling as hardware architecture
- ✅ Double-entry transaction validation (conservation laws)
- ✅ Hardware-style signal flow tracing and analysis
- ✅ Interactive transaction simulation and exploration
- ✅ Complete demo scenarios showcasing the hardware analogy
- ✅ Account balance monitoring and circuit state reports
- ✅ Business rule validation using circuit laws
- ✅ LLM-powered financial reasoning (when model provided)

## Testing

Run the comprehensive test suite:
```bash
./test-financial-logic
```

Tests validate:
- Chart of Accounts initialization
- Transaction balancing (conservation laws)
- Signal routing through financial circuits
- Hardware analogy terminology and signal polarity
