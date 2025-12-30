#!/bin/bash

# Run Streamlit with AWS profile 'test'
export AWS_PROFILE=test
export AWS_DEFAULT_REGION=us-east-1

echo "=============================================="
echo "🔐 Login Credentials:"
echo "Username: testuser"
echo "Password: MyPassword123!"
echo "=============================================="
echo ""
echo "📌 Access URL: http://localhost:8501"
echo "📌 For WSL: Check VS Code PORTS tab if localhost doesn't work"
echo ""

streamlit run main.py --server.address=0.0.0.0 --server.port=8501
