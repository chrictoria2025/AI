┌──────────────────────────────────────────────────────────┐
│  AgentCore Runtime Container                             │
│                                                          │
│  ┌────────────────────────────────────────────┐          │
│  │  Agent Code (lab4_runtime.py)              │          │
│  │                                            │          │
│  │  Local Python Tools:                       │          │
│  │  • get_product_info()    ◄─────────────────┼─ In-process  (HC Python)
│  │  • get_return_policy()   ◄─────────────────┼─ In-process
│  │  • get_technical_support() ◄───────────────┼─ In-process → KB
│  │                                             │          │
│  │  MCP Client:                                │          │
│  │  • mcp_client.list_tools_sync() ──────┐     │          │
│  └────────────────────────────────────-──┼─────┘          │
└───────────────────────────────────────────┼────────────────┘
                                           │
                                           │ HTTP/MCP Protocol
                                           │ (Authorization: Bearer JWT)
                                           ▼
                    ┌───────────────────────────────────────┐
                    │  AgentCore Gateway                    │
                    │  (MCP Server - Protocol Translation)  │
                    │                                        │
                    │  • Receives MCP requests               │
                    │  • Validates JWT token (Cognito)      │
                    │  • Translates MCP → Lambda event      │
                    │  • Routes to Lambda target             │
                    └────────────┬──────────────────────────┘
                                 │
                                 │ Lambda Invocation
                                 │ (event + context with tool name)
                                 ▼
                    ┌──────────────────────────────────┐
                    │  Lambda Function                 │
                    │  (lambda_function.py)            │
                    │                                   │
                    │  • check_warranty.py             │
                    │    → DynamoDB query              │
                    │                                   │
                    │  • web_search.py                 │
                    │    → DDGS API call               │
                    └──────────────────────────────────┘



🔑 Key Points:
1. Lambda is NOT an MCP server directly - it's just a normal Lambda function
2. AgentCore Gateway acts as the MCP server - it wraps Lambda and exposes it via  MCP protocol
3. Agent uses MCP Client - connects to Gateway, discovers tools, makes MCP calls
4. Gateway translates - MCP requests → Lambda events → MCP responses
5. Authentication - JWT tokens from Cognito validated by Gateway
6. Tool discovery - mcp_client.list_tools_sync() fetches available tools from Gateway


2. get_technical_support() - ✅ Bedrock Knowledge Base
Data Source: Amazon Bedrock Knowledge Base with S3 Vector Store

How it works:

┌─────────────────────────────────────────────────────────────┐
│  CloudFormation Stack (prerequisite/infrastructure.yaml)    │
│                                                              │
│  1. Lambda uploads 6 technical docs to S3:                  │
│     • troubleshooting-guide.txt                             │
│     • laptop-maintenance-guide.txt                          │
│     • smartphone-setup-guide.txt                            │
│     • monitor-calibration-guide.txt                         │
│     • wireless-connectivity-guide.txt                       │
│     • warranty-service-guide.txt                            │
│                                                              │
│  2. Creates S3 Vector Bucket for embeddings                 │
│                                                              │
│  3. Creates Bedrock Knowledge Base:                         │
│     • Embedding model: amazon.titan-embed-text-v2:0         │
│     • Vector dimension: 1024                                │
│     • Chunking: 200 tokens, 10% overlap                     │
│                                                              │
│  4. Creates Data Source pointing to S3 bucket               │
│                                                              │
│  5. Stores KB ID in SSM Parameter Store:                    │
│     /{account-id}-{region}/kb/knowledge-base-id             │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│  get_technical_support(issue_description)                   │
│                                                              │
│  1. Retrieves KB ID from SSM Parameter Store                │
│  2. Calls Bedrock RetrieveAndGenerate API                   │
│  3. KB searches vector embeddings for relevant docs         │
│  4. Returns top 3 results with relevance score > 0.4        │
└─────────────────────────────────────────────────────────────┘



Recommended Demo Flow
Start with:

"What tools do you have?" → Shows agent's self-awareness
"What are your laptop specifications?" → Quick response, hardcoded data
"What's your return policy for electronics?" → Another fast, hardcoded response
"My laptop won't turn on" → Semantic search from Knowledge Base (slower, more intelligent)
"Can you check the warranty status for serial number ABC12345678?" → External system integration via MCP
"Search for latest laptop processor benchmarks" → Web search for current info
This sequence shows: tool listing → fast local tools → intelligent KB search → external integrations.


1. show lambda - 
2. dynomo DB
3. AgentCore: Runtime - agent  --> Gateways ----> memory strategy >>> Policy >>> identity (RBAC) >>> Eval (correctness/GoalSuccessRate/ToolSelectionAccuracy)