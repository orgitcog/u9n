#include "llama.h"

#include <cassert>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

// Financial Account Types - mapped to hardware subsystems
enum class AccountType {
    ASSET,      // Input registers/storage
    LIABILITY,  // Output buffers/obligations
    EQUITY,     // Core processing unit
    REVENUE,    // Signal generators/inputs
    EXPENSE     // Signal consumers/outputs
};

// Account Node - represents a hardware pin/register
struct Account {
    std::string code;           // Pin/node identifier
    std::string name;           // Human readable name
    AccountType type;           // Hardware subsystem type
    double balance;             // Current signal level/voltage
    std::string parent_code;    // Parent node in hierarchy
    std::vector<std::string> children; // Child nodes
    bool is_debit_normal;       // Signal polarity (true = positive signal increases balance)
    
    Account() : code(""), name(""), type(AccountType::ASSET), balance(0.0), parent_code(""), is_debit_normal(true) {}
    
    Account(const std::string& c, const std::string& n, AccountType t, const std::string& parent = "")
        : code(c), name(n), type(t), balance(0.0), parent_code(parent), is_debit_normal(t == AccountType::ASSET || t == AccountType::EXPENSE) {}
};

// Transaction Entry - represents signal flow between nodes
struct TransactionEntry {
    std::string account_code;   // Target node
    double debit_amount;        // Positive signal flow
    double credit_amount;       // Negative signal flow
    std::string description;    // Signal description
};

// Transaction - represents a complete signal routing operation
struct Transaction {
    std::string id;
    std::string description;
    std::vector<TransactionEntry> entries;
    std::string timestamp;
    
    // Validate double-entry (conservation law)
    bool is_balanced() const {
        double total_debits = 0.0, total_credits = 0.0;
        for (const auto& entry : entries) {
            total_debits += entry.debit_amount;
            total_credits += entry.credit_amount;
        }
        return std::abs(total_debits - total_credits) < 0.01; // Allow small floating point errors
    }
};

// Chart of Accounts - the virtual hardware architecture
class ChartOfAccounts {
private:
    std::map<std::string, Account> accounts;
    std::vector<Transaction> transaction_log;
    
public:
    // Initialize with standard chart of accounts
    void initialize_standard_coa() {
        // Asset Subsystem (Input/Storage)
        add_account("1000", "Assets", AccountType::ASSET);
        add_account("1100", "Current Assets", AccountType::ASSET, "1000");
        add_account("1101", "Cash", AccountType::ASSET, "1100");
        add_account("1102", "Accounts Receivable", AccountType::ASSET, "1100");
        add_account("1103", "Inventory", AccountType::ASSET, "1100");
        add_account("1200", "Fixed Assets", AccountType::ASSET, "1000");
        add_account("1201", "Equipment", AccountType::ASSET, "1200");
        add_account("1202", "Buildings", AccountType::ASSET, "1200");
        
        // Liability Subsystem (Output/Obligations)
        add_account("2000", "Liabilities", AccountType::LIABILITY);
        add_account("2100", "Current Liabilities", AccountType::LIABILITY, "2000");
        add_account("2101", "Accounts Payable", AccountType::LIABILITY, "2100");
        add_account("2102", "Short-term Loans", AccountType::LIABILITY, "2100");
        add_account("2200", "Long-term Liabilities", AccountType::LIABILITY, "2000");
        add_account("2201", "Long-term Loans", AccountType::LIABILITY, "2200");
        
        // Equity Subsystem (Core Processing)
        add_account("3000", "Equity", AccountType::EQUITY);
        add_account("3100", "Owner's Equity", AccountType::EQUITY, "3000");
        add_account("3200", "Retained Earnings", AccountType::EQUITY, "3000");
        
        // Revenue Subsystem (Signal Sources)
        add_account("4000", "Revenue", AccountType::REVENUE);
        add_account("4100", "Sales Revenue", AccountType::REVENUE, "4000");
        add_account("4200", "Service Revenue", AccountType::REVENUE, "4000");
        
        // Expense Subsystem (Signal Sinks)
        add_account("5000", "Expenses", AccountType::EXPENSE);
        add_account("5100", "Operating Expenses", AccountType::EXPENSE, "5000");
        add_account("5101", "Salaries Expense", AccountType::EXPENSE, "5100");
        add_account("5102", "Rent Expense", AccountType::EXPENSE, "5100");
        add_account("5103", "Utilities Expense", AccountType::EXPENSE, "5100");
    }
    
    void add_account(const std::string& code, const std::string& name, AccountType type, const std::string& parent = "") {
        accounts[code] = Account(code, name, type, parent);
        if (!parent.empty() && accounts.find(parent) != accounts.end()) {
            accounts[parent].children.push_back(code);
        }
    }
    
    bool account_exists(const std::string& code) const {
        return accounts.find(code) != accounts.end();
    }
    
    Account* get_account(const std::string& code) {
        auto it = accounts.find(code);
        return (it != accounts.end()) ? &it->second : nullptr;
    }
    
    // Process transaction (route signals through the hardware)
    bool process_transaction(const Transaction& transaction) {
        if (!transaction.is_balanced()) {
            std::cerr << "Error: Transaction violates conservation law (not balanced)\n";
            return false;
        }
        
        // Validate all accounts exist
        for (const auto& entry : transaction.entries) {
            if (!account_exists(entry.account_code)) {
                std::cerr << "Error: Account node " << entry.account_code << " not found in circuit\n";
                return false;
            }
        }
        
        // Apply signal routing
        for (const auto& entry : transaction.entries) {
            Account* account = get_account(entry.account_code);
            if (account->is_debit_normal) {
                account->balance += entry.debit_amount - entry.credit_amount;
            } else {
                account->balance += entry.credit_amount - entry.debit_amount;
            }
        }
        
        // Log the transaction
        transaction_log.push_back(transaction);
        return true;
    }
    
    // Generate hardware state report
    std::string generate_balance_sheet() const {
        std::stringstream ss;
        ss << "=== FINANCIAL CIRCUIT STATE REPORT ===\n\n";
        
        ss << "ASSET SUBSYSTEM (Input/Storage Nodes):\n";
        print_account_tree(ss, "1000", 0);
        
        ss << "\nLIABILITY SUBSYSTEM (Output/Obligation Nodes):\n";
        print_account_tree(ss, "2000", 0);
        
        ss << "\nEQUITY SUBSYSTEM (Core Processing Unit):\n";
        print_account_tree(ss, "3000", 0);
        
        return ss.str();
    }
    
    std::string generate_income_statement() const {
        std::stringstream ss;
        ss << "=== SIGNAL FLOW ANALYSIS REPORT ===\n\n";
        
        ss << "REVENUE SUBSYSTEM (Signal Sources):\n";
        print_account_tree(ss, "4000", 0);
        
        ss << "\nEXPENSE SUBSYSTEM (Signal Sinks):\n";
        print_account_tree(ss, "5000", 0);
        
        // Calculate net signal flow
        double total_revenue = calculate_subtotal("4000");
        double total_expenses = calculate_subtotal("5000");
        double net_income = total_revenue - total_expenses;
        
        ss << "\n--- SIGNAL FLOW SUMMARY ---\n";
        ss << "Total Input Signals (Revenue): $" << std::fixed << std::setprecision(2) << total_revenue << "\n";
        ss << "Total Output Signals (Expenses): $" << std::fixed << std::setprecision(2) << total_expenses << "\n";
        ss << "Net Signal Flow: $" << std::fixed << std::setprecision(2) << net_income << "\n";
        
        return ss.str();
    }
    
    // Trace signal path through the circuit
    std::string trace_transaction_path(const Transaction& transaction) const {
        std::stringstream ss;
        ss << "=== SIGNAL ROUTING TRACE ===\n";
        ss << "Transaction ID: " << transaction.id << "\n";
        ss << "Description: " << transaction.description << "\n\n";
        
        for (const auto& entry : transaction.entries) {
            auto account_it = accounts.find(entry.account_code);
            if (account_it != accounts.end()) {
                const Account& account = account_it->second;
                ss << "Node: " << account.code << " (" << account.name << ")\n";
                ss << "  Subsystem: " << get_account_type_name(account.type) << "\n";
                ss << "  Signal Flow: ";
                if (entry.debit_amount > 0) {
                    ss << "+" << entry.debit_amount << " (Debit/Positive)";
                }
                if (entry.credit_amount > 0) {
                    ss << "-" << entry.credit_amount << " (Credit/Negative)";
                }
                ss << "\n  Current State: $" << std::fixed << std::setprecision(2) << account.balance << "\n\n";
            }
        }
        
        return ss.str();
    }
    
    double get_balance(const std::string& code) const {
        auto it = accounts.find(code);
        return (it != accounts.end()) ? it->second.balance : 0.0;
    }
    
    size_t get_transaction_count() const {
        return transaction_log.size();
    }
    
private:
    void print_account_tree(std::stringstream& ss, const std::string& account_code, int depth) const {
        auto it = accounts.find(account_code);
        if (it == accounts.end()) return;
        
        const Account& account = it->second;
        
        // Indentation for hierarchy
        for (int i = 0; i < depth; i++) ss << "  ";
        
        ss << account.code << " " << account.name << ": $" 
           << std::fixed << std::setprecision(2) << account.balance << "\n";
        
        // Print children
        for (const std::string& child_code : account.children) {
            print_account_tree(ss, child_code, depth + 1);
        }
    }
    
    double calculate_subtotal(const std::string& account_code) const {
        auto it = accounts.find(account_code);
        if (it == accounts.end()) return 0.0;
        
        const Account& account = it->second;
        double total = account.balance;
        
        for (const std::string& child_code : account.children) {
            total += calculate_subtotal(child_code);
        }
        
        return total;
    }
    
    std::string get_account_type_name(AccountType type) const {
        switch (type) {
            case AccountType::ASSET: return "Asset (Storage/Input)";
            case AccountType::LIABILITY: return "Liability (Output/Buffer)";
            case AccountType::EQUITY: return "Equity (Core Processing)";
            case AccountType::REVENUE: return "Revenue (Signal Source)";
            default: return "Unknown";
        }
    }
};

// Financial LLM Prompt Templates
class FinancialPrompts {
public:
    static std::string generate_hardware_analysis_prompt(const std::string& context, const std::string& query) {
        return "You are a financial circuit analyst examining a Chart of Accounts modeled as virtual hardware.\n\n"
               "HARDWARE ARCHITECTURE:\n"
               "- Accounts are electronic nodes/pins with voltage levels (balances)\n"
               "- Transactions are signal routing operations between nodes\n"
               "- Account types map to hardware subsystems:\n"
               "  * Assets = Input/Storage registers\n"
               "  * Liabilities = Output buffers/obligations\n"
               "  * Equity = Core processing unit\n"
               "  * Revenue = Signal generators/sources\n"
               "  * Expenses = Signal consumers/sinks\n"
               "- Double-entry accounting = Conservation of signal/charge law\n\n"
               "CURRENT CIRCUIT STATE:\n" + context + "\n\n"
               "ANALYSIS REQUEST:\n" + query + "\n\n"
               "Please analyze this financial circuit using hardware engineering principles. "
               "Explain signal flows, node interactions, subsystem behavior, and any potential "
               "circuit violations or optimizations. Use electronic/hardware terminology where appropriate.";
    }
    
    static std::string generate_transaction_routing_prompt(const std::string& transaction_trace, const std::string& query) {
        return "You are analyzing signal routing in a financial hardware circuit.\n\n"
               "SIGNAL ROUTING TRACE:\n" + transaction_trace + "\n\n"
               "ROUTING ANALYSIS REQUEST:\n" + query + "\n\n"
               "Analyze this signal routing operation as if it were happening in electronic hardware. "
               "Explain the signal path, node state changes, and validate that the routing follows "
               "proper circuit laws (conservation of charge/signal). Identify any potential issues "
               "or improvements in the signal routing design.";
    }
    
    static std::string generate_circuit_validation_prompt(const std::string& financial_state) {
        return "You are a circuit validation engineer examining a financial hardware system.\n\n"
               "CIRCUIT STATE:\n" + financial_state + "\n\n"
               "Please perform a comprehensive circuit validation:\n"
               "1. Check signal conservation across all subsystems\n"
               "2. Identify any voltage imbalances or anomalies\n"
               "3. Analyze subsystem performance and efficiency\n"
               "4. Suggest circuit optimizations or reconfigurations\n"
               "5. Flag any potential circuit failures or instabilities\n\n"
               "Report your findings using hardware engineering terminology and standards.";
    }
};

// Main application class
class FinancialSimulator {
private:
    std::string model_path;
    int n_predict = 512;
    int n_gpu_layers = 0;
    llama_model* model = nullptr;
    llama_context* ctx = nullptr;
    const llama_vocab* vocab = nullptr;
    ChartOfAccounts coa;
    
public:
    bool initialize(const std::string& model_file, int predict_tokens = 512, int gpu_layers = 0) {
        model_path = model_file;
        n_predict = predict_tokens;
        n_gpu_layers = gpu_layers;
        
        // Load dynamic backends
        ggml_backend_load_all();
        
        // Initialize llama model
        llama_model_params model_params = llama_model_default_params();
        model_params.n_gpu_layers = n_gpu_layers;
        
        model = llama_model_load_from_file(model_path.c_str(), model_params);
        if (!model) {
            std::cerr << "Error: Could not load model from " << model_path << std::endl;
            return false;
        }
        
        // Get vocab
        vocab = llama_model_get_vocab(model);
        
        // Create context
        llama_context_params ctx_params = llama_context_default_params();
        ctx_params.n_ctx = 4096;  // context size
        ctx_params.n_batch = 512; // batch size for prompt processing
        
        ctx = llama_init_from_model(model, ctx_params);
        if (!ctx) {
            std::cerr << "Error: Could not create context" << std::endl;
            return false;
        }
        
        // Initialize chart of accounts
        coa.initialize_standard_coa();
        
        std::cout << "Financial Circuit Simulator initialized successfully!\n";
        std::cout << "Hardware architecture loaded with standard Chart of Accounts.\n\n";
        
        return true;
    }
    
    void initialize_coa_only() {
        // Initialize just the chart of accounts without LLM
        coa.initialize_standard_coa();
        std::cout << "Financial Circuit initialized (CoA only - no LLM)\n";
        std::cout << "Hardware architecture loaded with standard Chart of Accounts.\n\n";
    }
    
    ~FinancialSimulator() {
        if (ctx) llama_free(ctx);
        if (model) llama_model_free(model);
    }
    
    ChartOfAccounts& get_coa() {
        return coa;
    }
    
    void run_interactive_simulation() {
        std::cout << "=== INTERACTIVE FINANCIAL CIRCUIT SIMULATOR ===\n";
        std::cout << "Commands:\n";
        std::cout << "  balance-sheet    - Show current circuit state\n";
        std::cout << "  income-statement - Show signal flow analysis\n";
        std::cout << "  transaction      - Process a new signal routing\n";
        std::cout << "  quick-demo       - Run quick demo transactions\n";
        std::cout << "  analyze [query]  - Analyze circuit using LLM\n";
        std::cout << "  trace [tx_id]    - Trace specific transaction\n";
        std::cout << "  validate         - Validate circuit integrity\n";
        std::cout << "  accounts         - List all account nodes\n";
        std::cout << "  reset            - Reset circuit to initial state\n";
        std::cout << "  quit             - Exit simulator\n\n";
        
        std::string input;
        while (true) {
            std::cout << "FinSim> ";
            std::getline(std::cin, input);
            
            if (input == "quit" || input == "exit") {
                break;
            } else if (input == "balance-sheet") {
                std::cout << coa.generate_balance_sheet() << std::endl;
            } else if (input == "income-statement") {
                std::cout << coa.generate_income_statement() << std::endl;
            } else if (input == "transaction") {
                process_interactive_transaction();
            } else if (input == "quick-demo") {
                run_quick_demo();
            } else if (input.substr(0, 7) == "analyze") {
                std::string query = input.length() > 8 ? input.substr(8) : "Analyze the current financial circuit state";
                analyze_with_llm(query);
            } else if (input == "validate") {
                validate_circuit_with_llm();
            } else if (input == "accounts") {
                list_all_accounts();
            } else if (input == "reset") {
                reset_circuit();
            } else if (input == "help") {
                std::cout << "Available commands: balance-sheet, income-statement, transaction, quick-demo, analyze, validate, accounts, reset, quit\n";
            } else {
                std::cout << "Unknown command. Type 'help' for available commands.\n";
            }
        }
    }
    
private:
    void process_interactive_transaction() {
        std::cout << "=== TRANSACTION ENTRY (Signal Routing) ===\n";
        
        Transaction tx;
        tx.id = "TX" + std::to_string(time(nullptr));
        
        std::cout << "Description: ";
        std::getline(std::cin, tx.description);
        
        std::cout << "Enter transaction entries (account_code debit_amount credit_amount description)\n";
        std::cout << "Enter empty line to finish:\n";
        
        std::string line;
        while (std::getline(std::cin, line) && !line.empty()) {
            std::istringstream iss(line);
            TransactionEntry entry;
            iss >> entry.account_code >> entry.debit_amount >> entry.credit_amount;
            std::getline(iss, entry.description);
            tx.entries.push_back(entry);
        }
        
        if (coa.process_transaction(tx)) {
            std::cout << "✓ Signal routing completed successfully!\n";
            std::cout << coa.trace_transaction_path(tx) << std::endl;
        } else {
            std::cout << "✗ Signal routing failed - check circuit laws!\n";
        }
    }
    
    void analyze_with_llm(const std::string& query) {
        std::string context = coa.generate_balance_sheet() + "\n" + coa.generate_income_statement();
        std::string prompt = FinancialPrompts::generate_hardware_analysis_prompt(context, query);
        
        std::cout << "Analyzing circuit with LLM...\n\n";
        generate_response(prompt);
    }
    
    void validate_circuit_with_llm() {
        std::string financial_state = coa.generate_balance_sheet() + "\n" + coa.generate_income_statement();
        std::string prompt = FinancialPrompts::generate_circuit_validation_prompt(financial_state);
        
        std::cout << "Validating circuit integrity with LLM...\n\n";
        generate_response(prompt);
    }
    
    void generate_response(const std::string& prompt) {
        // Tokenize the prompt
        const int n_prompt = -llama_tokenize(vocab, prompt.c_str(), prompt.size(), NULL, 0, true, true);
        std::vector<llama_token> prompt_tokens(n_prompt);
        if (llama_tokenize(vocab, prompt.c_str(), prompt.size(), prompt_tokens.data(), n_prompt, true, true) < 0) {
            std::cerr << "Error: Failed to tokenize prompt\n";
            return;
        }
        
        // Create batch for prompt processing
        llama_batch batch = llama_batch_get_one(prompt_tokens.data(), prompt_tokens.size());
        
        // Evaluate the prompt
        if (llama_decode(ctx, batch) != 0) {
            std::cerr << "Error: Failed to decode prompt\n";
            return;
        }
        
        // Initialize sampler
        auto sparams = llama_sampler_chain_default_params();
        sparams.no_perf = false;
        llama_sampler* sampler = llama_sampler_chain_init(sparams);
        llama_sampler_chain_add(sampler, llama_sampler_init_greedy());
        
        // Generate response
        std::cout << "LLM Analysis:\n";
        int n_decode = 0;
        for (int i = 0; i < n_predict; i++) {
            // Sample next token
            llama_token new_token = llama_sampler_sample(sampler, ctx, -1);
            
            // Check for end of generation
            if (llama_vocab_is_eog(vocab, new_token)) {
                break;
            }
            
            // Convert token to text and print
            char buf[128];
            int n = llama_token_to_piece(vocab, new_token, buf, sizeof(buf), 0, true);
            if (n < 0) {
                std::cerr << "\nError: Failed to convert token to piece\n";
                break;
            }
            std::string s(buf, n);
            std::cout << s;
            std::cout.flush();
            
            // Prepare for next iteration
            batch = llama_batch_get_one(&new_token, 1);
            if (llama_decode(ctx, batch) != 0) {
                std::cerr << "\nError: Failed to decode token\n";
                break;
            }
            
            n_decode++;
        }
        std::cout << "\n\n";
        
        llama_sampler_free(sampler);
    }
    
    void run_quick_demo() {
        std::cout << "Running quick demonstration scenario...\n\n";
        
        // Simple investment and revenue scenario
        Transaction investment;
        investment.id = "QUICK-001";
        investment.description = "Owner investment - power injection";
        investment.entries = {
            {"1101", 10000.0, 0.0, "Cash injection"},
            {"3100", 0.0, 10000.0, "Owner's equity"}
        };
        
        if (coa.process_transaction(investment)) {
            std::cout << "✓ Investment transaction completed\n";
        }
        
        Transaction revenue;
        revenue.id = "QUICK-002";
        revenue.description = "Service revenue - signal generation";
        revenue.entries = {
            {"1101", 2500.0, 0.0, "Cash received"},
            {"4100", 0.0, 2500.0, "Service revenue"}
        };
        
        if (coa.process_transaction(revenue)) {
            std::cout << "✓ Revenue transaction completed\n";
        }
        
        std::cout << "\nQuick demo completed! Check balance-sheet to see results.\n";
    }
    
    void list_all_accounts() {
        std::cout << "=== FINANCIAL CIRCUIT NODE DIRECTORY ===\n";
        std::cout << "Available account nodes:\n";
        std::cout << "Asset Subsystem:\n";
        std::cout << "  1101 - Cash (Primary storage node)\n";
        std::cout << "  1102 - Accounts Receivable (Pending input buffer)\n";
        std::cout << "  1103 - Inventory (Material buffer)\n";
        std::cout << "  1201 - Equipment (Processing equipment)\n";
        std::cout << "  1202 - Buildings (Infrastructure)\n";
        std::cout << "Liability Subsystem:\n";
        std::cout << "  2101 - Accounts Payable (Output queue)\n";
        std::cout << "  2102 - Short-term Loans (Temporary power)\n";
        std::cout << "  2201 - Long-term Loans (Extended power)\n";
        std::cout << "Equity Subsystem:\n";
        std::cout << "  3100 - Owner's Equity (Control processor)\n";
        std::cout << "  3200 - Retained Earnings (Gain storage)\n";
        std::cout << "Revenue Subsystem:\n";
        std::cout << "  4100 - Sales Revenue (Primary generator)\n";
        std::cout << "  4200 - Service Revenue (Secondary generator)\n";
        std::cout << "Expense Subsystem:\n";
        std::cout << "  5101 - Salaries Expense (HR consumption)\n";
        std::cout << "  5102 - Rent Expense (Infrastructure consumption)\n";
        std::cout << "  5103 - Utilities Expense (Utility consumption)\n";
    }
    
    void reset_circuit() {
        coa = ChartOfAccounts(); // Reset to fresh state
        coa.initialize_standard_coa();
        std::cout << "Circuit reset to initial state. All signal levels zeroed.\n";
    }
};

// Demo transactions for testing
void run_demo_scenario(FinancialSimulator& sim) {
    std::cout << "Running demo financial circuit scenario...\n\n";
    
    // Demo transactions that showcase the hardware analogy
    std::cout << "=== FINANCIAL CIRCUIT DEMO SCENARIO ===\n";
    std::cout << "Simulating a small business financial circuit...\n\n";
    
    // Get access to the CoA for demonstration
    auto& coa = sim.get_coa();
    
    std::cout << "1. INITIAL CIRCUIT STATE:\n";
    std::cout << coa.generate_balance_sheet() << "\n";
    
    // Transaction 1: Owner investment (power injection)
    std::cout << "2. POWER INJECTION - Owner Investment:\n";
    Transaction investment;
    investment.id = "DEMO-001";
    investment.description = "Initial capital injection into the financial circuit";
    investment.entries = {
        {"1101", 50000.0, 0.0, "Cash injection at primary input node"},
        {"3100", 0.0, 50000.0, "Power source registration in core processing unit"}
    };
    
    if (coa.process_transaction(investment)) {
        std::cout << "✓ Signal routing completed - $50,000 injected into circuit\n";
        std::cout << coa.trace_transaction_path(investment) << "\n";
    }
    
    // Transaction 2: Equipment purchase (internal routing)
    std::cout << "3. INTERNAL SIGNAL ROUTING - Equipment Purchase:\n";
    coa.add_account("1201", "Office Equipment", AccountType::ASSET, "1200");
    Transaction equipment;
    equipment.id = "DEMO-002";
    equipment.description = "Internal circuit reconfiguration - asset transformation";
    equipment.entries = {
        {"1201", 15000.0, 0.0, "Equipment node activation"},
        {"1101", 0.0, 15000.0, "Cash node signal reduction"}
    };
    
    if (coa.process_transaction(equipment)) {
        std::cout << "✓ Internal routing completed - signal redistributed in asset subsystem\n";
        std::cout << coa.trace_transaction_path(equipment) << "\n";
    }
    
    // Transaction 3: Revenue generation (signal amplification)
    std::cout << "4. SIGNAL AMPLIFICATION - Revenue Generation:\n";
    Transaction revenue;
    revenue.id = "DEMO-003";
    revenue.description = "Signal amplification through revenue generation subsystem";
    revenue.entries = {
        {"1101", 8000.0, 0.0, "Cash input buffer receives amplified signal"},
        {"4100", 0.0, 8000.0, "Revenue generator produces positive signal"}
    };
    
    if (coa.process_transaction(revenue)) {
        std::cout << "✓ Signal amplification successful - circuit gained $8,000 energy\n";
        std::cout << coa.trace_transaction_path(revenue) << "\n";
    }
    
    // Transaction 4: Expense (signal consumption)
    std::cout << "5. SIGNAL CONSUMPTION - Operating Expenses:\n";
    Transaction expense;
    expense.id = "DEMO-004";
    expense.description = "Signal consumption by expense subsystem modules";
    expense.entries = {
        {"5101", 3000.0, 0.0, "Salary expense module consumption"},
        {"5102", 1200.0, 0.0, "Rent expense module consumption"},
        {"1101", 0.0, 4200.0, "Cash node signal reduction for consumption"}
    };
    
    // Add rent expense account
    coa.add_account("5102", "Rent Expense", AccountType::EXPENSE, "5100");
    
    if (coa.process_transaction(expense)) {
        std::cout << "✓ Signal consumption completed - circuit consumed $4,200 energy\n";
        std::cout << coa.trace_transaction_path(expense) << "\n";
    }
    
    // Transaction 5: Loan (external power injection)
    std::cout << "6. EXTERNAL POWER INJECTION - Bank Loan:\n";
    coa.add_account("2201", "Bank Loan", AccountType::LIABILITY, "2200");
    Transaction loan;
    loan.id = "DEMO-005";
    loan.description = "External power injection from banking power grid";
    loan.entries = {
        {"1101", 20000.0, 0.0, "Cash buffer receives external power"},
        {"2201", 0.0, 20000.0, "Liability module registers external power contract"}
    };
    
    if (coa.process_transaction(loan)) {
        std::cout << "✓ External power injection successful - $20,000 from banking grid\n";
        std::cout << coa.trace_transaction_path(loan) << "\n";
    }
    
    // Final circuit state
    std::cout << "7. FINAL CIRCUIT STATE ANALYSIS:\n";
    std::cout << coa.generate_balance_sheet() << "\n";
    std::cout << coa.generate_income_statement() << "\n";
    
    // Circuit analysis
    std::cout << "8. CIRCUIT ANALYSIS SUMMARY:\n";
    std::cout << "=== HARDWARE CIRCUIT PERFORMANCE METRICS ===\n";
    std::cout << "Total Signal Injections: $78,000 (Investment + Revenue + Loan)\n";
    std::cout << "Total Signal Consumption: $19,200 (Equipment + Expenses)\n";
    std::cout << "Net Circuit Gain: $58,800\n";
    std::cout << "Primary Storage Node (Cash): $" << coa.get_balance("1101") << "\n";
    std::cout << "Circuit Efficiency: " << std::fixed << std::setprecision(1) 
              << (1.0 - 19200.0/78000.0) * 100 << "%\n";
    std::cout << "Active Transaction Log Entries: " << coa.get_transaction_count() << "\n\n";
    
    std::cout << "Demo scenario completed! Circuit is operating efficiently.\n";
}

static void print_usage(int /* argc */, char** argv) {
    printf("\nFinancial Circuit Simulator - Chart of Accounts as Virtual Hardware\n");
    printf("\nexample usage:\n");
    printf("\n    %s -m model.gguf [options]\n", argv[0]);
    printf("\nRequired options:\n");
    printf("  -m MODEL_FILE           Path to GGUF model file\n");
    printf("\nOptional options:\n");
    printf("  -n N_PREDICT            Number of tokens to predict (default: 512)\n");
    printf("  -ngl N_GPU_LAYERS       Number of layers to offload to GPU (default: 0)\n");
    printf("  --interactive           Run in interactive mode\n");
    printf("  --demo                  Run demo scenario\n");
    printf("  --coa FILE              Load chart of accounts from JSON file\n");
    printf("  --trace DESCRIPTION     Trace specific transaction\n");
    printf("  -h, --help              Show this help message\n");
    printf("\n");
}

int main(int argc, char** argv) {
    std::string model_path;
    int n_predict = 512;
    int n_gpu_layers = 0;
    bool interactive = false;
    bool demo = false;
    std::string coa_file;
    std::string trace_description;
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-m" && i + 1 < argc) {
            model_path = argv[++i];
        } else if (arg == "-n" && i + 1 < argc) {
            try {
                n_predict = std::stoi(argv[++i]);
            } catch (...) {
                print_usage(argc, argv);
                return 1;
            }
        } else if (arg == "-ngl" && i + 1 < argc) {
            try {
                n_gpu_layers = std::stoi(argv[++i]);
            } catch (...) {
                print_usage(argc, argv);
                return 1;
            }
        } else if (arg == "--interactive") {
            interactive = true;
        } else if (arg == "--demo") {
            demo = true;
        } else if (arg == "--coa" && i + 1 < argc) {
            coa_file = argv[++i];
        } else if (arg == "--trace" && i + 1 < argc) {
            trace_description = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argc, argv);
            return 0;
        }
    }
    
    if (model_path.empty() && !demo && !interactive) {
        std::cerr << "Error: No model specified. Use -m MODEL_FILE (or --demo/--interactive for modes without LLM)\n";
        print_usage(argc, argv);
        return 1;
    }
    
    // Initialize simulator
    FinancialSimulator simulator;
    
    if (demo) {
        // Demo mode without LLM
        std::cout << "Running demo mode without LLM initialization...\n";
        simulator.initialize_coa_only();
        run_demo_scenario(simulator);
    } else if (interactive && model_path.empty()) {
        // Interactive mode without LLM
        std::cout << "Running interactive mode without LLM...\n";
        simulator.initialize_coa_only();
        simulator.run_interactive_simulation();
    } else {
        // Full mode with LLM
        if (!simulator.initialize(model_path, n_predict, n_gpu_layers)) {
            return 1;
        }
        
        if (interactive) {
            simulator.run_interactive_simulation();
        } else {
            std::cout << "Financial Circuit Simulator ready. Use --interactive for interactive mode.\n";
            std::cout << "Use --help for all available options.\n";
        }
    }
    
    return 0;
}