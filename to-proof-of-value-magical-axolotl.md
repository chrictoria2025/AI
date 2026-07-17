# POV Deployment Plan: ZPA App Connector on Azure (ESS-DEV Subscription)

## Context

**Goal**: Deploy 2 Zscaler Private Access (ZPA) App Connectors in ESS-DEV subscription to demonstrate connectivity and validate traffic routing through existing Fortinet firewall infrastructure.

**Infrastructure Setup**:
- **Subscription**: ESS-DEV (single subscription)
- **Region**: East US (eastus)
- **Existing subnet**: Already present with Fortinet firewall routing configured
- **New resource group**: Will be created for App Connector VMs and related resources
- **New VMs**: 2 App Connector instances

**Success criteria**: 
- App Connectors successfully register in ZPA
- Connectors route outbound traffic through Fortinet firewall
- Both connectors show "Connected" status in ZPA portal

---

## Recommended Approach: `ac` Example (Brownfield, Single Subscription)

### Why `ac` example?

| Factor | Why `ac` wins |
|---|---|
| **Architecture fit** | Brownfield designed for existing infrastructure; supports bring-your-own (BYO) resources |
| **Subnet reuse** | Binds to your existing subnet in ESS-DEV (no new subnet creation) |
| **New RG** | Creates dedicated resource group for connectors (clean separation) |
| **Firewall compatible** | Doesn't force NAT Gateway; connectors inherit firewall routing |
| **Cost** | Fixed 2 VMs; ~$150-180/month |
| **Simplicity** | Single subscription = no cross-subscription complexity |

**What `ac` example creates:**
- 1 new Resource Group (e.g., `rg-zpa-connectors-poc`)
- 2 App Connector VMs in your existing subnet
- NSGs for VM management
- App Connector Group and Provisioning Key in ZPA
- SSH key pair for management access

**What `ac` does NOT create** (because they already exist):
- VNet (your existing VNet in ESS-DEV)
- Subnet (your existing private subnet)
- Fortinet firewall (already in place)

**Monthly cost estimate**: ~$150-180
- 2x App Connector VMs (Standard_D4s_v5): ~$120
- NSGs, storage: minimal (~$10-15)
- **No Bastion, no NAT Gateway** (Fortinet handles egress)
- **Total: ~$150/month**

---

## Prerequisites Checklist

### ESS-DEV Subscription Setup

- [ ] **Subscription ID**: Copy from Azure Portal
  - Docs: https://portal.azure.com/#blade/Microsoft_Azure_Billing/SubscriptionsBlade
  
- [ ] **Existing VNet details**:
  - [ ] VNet name (e.g., `ess-dev-vnet`)
  - [ ] VNet CIDR block (e.g., `10.10.0.0/16`)
  - [ ] Resource Group containing VNet (e.g., `rg-network-essdev`)

- [ ] **Existing subnet details**:
  - [ ] Subnet name (e.g., `app-subnet` or `private-subnet`)
  - [ ] Subnet CIDR (e.g., `10.10.20.0/24`)
  - [ ] Confirm it's **private** (no public IPs assigned)
  - [ ] Confirm **Fortinet firewall is gateway** (route table points to Fortinet IP)

- [ ] **Fortinet firewall details**:
  - [ ] Firewall private IP (e.g., `10.10.1.4`)
  - [ ] Verify firewall is set as default gateway for subnet
  - [ ] Verify firewall policy allows outbound HTTPS (TCP 443) to Zscaler

- [ ] **Service Principal created** with credentials:
  - [ ] Application (Client) ID
  - [ ] Directory (Tenant) ID
  - [ ] Client Secret
  - Docs: https://learn.microsoft.com/en-us/azure/active-directory/develop/howto-create-service-principal-portal

- [ ] **Service Principal permissions**:
  - [ ] **Contributor** role on **ESS-DEV subscription** (enables VM creation)
  - [ ] Can verify with: `az role assignment list --assignee <client-id> --output table`

- [ ] **Marketplace image agreement** (one-time):
  ```bash
  az vm image terms accept --urn zscaler:zscaler-private-access:zpa-con-azure:latest
  ```

### Zscaler ZPA Setup

- [ ] **Active ZPA subscription** with admin portal access

- [ ] **Authentication method identified**:
  - [ ] **OAuth2 (Zidentity)** — Recommended. Have ready:
    - [ ] Client ID (from ZPA admin portal)
    - [ ] Client Secret (from ZPA admin portal)
    - [ ] Customer ID (tenant identifier)
    - [ ] Vanity domain (your tenant's domain, e.g., `acme.private.zscaler.com`)
  
  - [ ] **Legacy Provisioning Key** — Older tenants. Have ready:
    - [ ] ZPA Client ID (API)
    - [ ] ZPA Client Secret (API)
    - [ ] ZPA Customer ID

### Local Machine

- [ ] **Terraform v1.1.9+** installed
  ```bash
  terraform --version  # Should show v1.x.x
  ```

- [ ] **Azure CLI** installed
  ```bash
  az --version
  ```

- [ ] **Repository cloned**:
  ```bash
  git clone https://github.com/zscaler/terraform-azurerm-zpa-app-connector-modules.git
  cd terraform-azurerm-zpa-app-connector-modules/examples
  ```

---

## Deployment Process

### Step 1: Prepare Configuration

```bash
# Navigate to ac example directory
cd terraform-azurerm-zpa-app-connector-modules/examples/ac
```

### Step 2: Edit `terraform.tfvars`

Open `terraform.tfvars` and customize these sections:

```hcl
# ===== AZURE SUBSCRIPTION & REGION =====
arm_subscription_id = "ESS-DEV-SUBSCRIPTION-ID"  # From Azure Portal
arm_location        = "eastus"                    # EAST US region

# ===== NEW RESOURCE GROUP =====
resource_group_name = "rg-zpa-connectors-poc"    # New RG name (customize as needed)

# ===== BRING-YOUR-OWN VNET & SUBNET =====
byo_vnet       = true
byo_vnet_name  = "ess-dev-vnet"           # Your existing VNet name in ESS-DEV
byo_vnet_rg_name = "rg-network-essdev"    # RG where your existing VNet lives

byo_subnets    = true
byo_subnet_names = ["app-subnet"]         # Your existing private subnet name

# ===== APP CONNECTOR SETTINGS =====
app_connector_count      = 2               # Deploy exactly 2 connectors
acvm_instance_type       = "Standard_D4s_v5"  # 4-core Intel, ~$60/month each

# App Connector Group (auto-created in ZPA)
app_connector_group_name        = "ESS-DEV-AC-Group"
app_connector_group_location    = "San Jose, CA, USA"  # Adjust if desired
app_connector_group_description = "POV - ESS-DEV Subscription"

# ===== ONBOARDING METHOD =====
onboarding_method = "oauth"  # or "provisioning_key" if using legacy

# ===== DISABLE NAT GATEWAY =====
# Since Fortinet handles all egress, no need for NAT Gateway
nat_gateway_enabled = false
```

### Step 3: Set Environment Variables

```bash
# Azure authentication (ESS-DEV subscription)
export ARM_SUBSCRIPTION_ID="ESS-DEV-SUBSCRIPTION-ID"
export ARM_CLIENT_ID="<service-principal-client-id>"
export ARM_CLIENT_SECRET="<service-principal-secret>"
export ARM_TENANT_ID="<tenant-id>"

# Zscaler authentication (ZPA API)
# Option A: OAuth2 (Zidentity)
export ZPA_CLIENT_ID="<zidentity-client-id>"
export ZPA_CLIENT_SECRET="<zidentity-client-secret>"
export ZPA_CUSTOMER_ID="<customer-id>"
export ZSCALER_VANITY_DOMAIN="<your-tenant-name>"

# Option B: Legacy Provisioning Key (if needed)
# export ZPA_CLIENT_ID="<api-client-id>"
# export ZPA_CLIENT_SECRET="<api-secret>"
# export ZPA_CUSTOMER_ID="<customer-id>"
# export ZPA_USE_LEGACY_CLIENT="true"
```

### Step 4: Deploy

```bash
# Navigate to ac example
cd examples/ac

# Initialize Terraform (first time only)
terraform init

# Review what will be created
terraform plan -out=tfplan

# Apply configuration
terraform apply tfplan

# When prompted, verify resources and type 'yes' to confirm
# ⏱️  Wait 35–50 minutes for full deployment
```

### Step 5: Capture Outputs

```bash
# After deployment completes, save important values
terraform output

# Key outputs:
# - app_connector_group_id: Reference for ZPA policies
# - resource_group_name: Where VMs were created
# - testbedconfig: Summary of deployment
```

---

## Verification Checklist (Post-Deployment)

### Timeline

| Stage | Duration | What's happening |
|-------|----------|---|
| Terraform apply | 5–10 min | VMs provisioned in ESS-DEV, new RG created |
| VM boot | 10–15 min | RHEL 9 OS startup, network configuration |
| Connector onboarding | 15–20 min | Agents register to ZPA via Fortinet firewall |
| **Total** | **30–45 min** | System ready |

### Verification Steps

1. **Verify Azure resources in ESS-DEV**:
   ```bash
   # Set context to ESS-DEV subscription
   az account set --subscription "ESS-DEV-SUBSCRIPTION-ID"
   
   # List resources in new RG
   az resource list -g rg-zpa-connectors-poc --output table
   
   # Expected: 2 VMs (e.g., zsac-ac-vm-0, zsac-ac-vm-1), NSGs, network interfaces
   ```

2. **Check ZPA Portal** (Primary verification):
   - Log into ZPA admin portal
   - Navigate: **Administration** → **App Connectors** → **Connector Groups**
   - Locate: **"ESS-DEV-AC-Group"** (or your custom group name)
   - **Expected state**: 
     - Both connectors show **"Connected"** ✅ green status
     - Health indicator: Green
     - Last seen: Recent timestamp
   - **Timeline**: Should appear within 15-20 minutes of terraform apply

3. **Monitor connector metrics** (optional):
   - Click group name to expand details
   - Check **Health**: Green checkmark
   - Check **Version**: Matches current ZPA version
   - Check **Latency**: Should be <100ms
   - Check **App Load**: Will be low initially (increases as traffic routes)

4. **Verify Fortinet routing** (optional):
   ```bash
   # SSH into an App Connector VM
   # (Requires NSG rule allowing TCP 22 from your IP)
   
   # Get connector private IP from Azure portal
   CONNECTOR_IP="10.10.20.10"  # Example
   ssh -i zsac-key.pem azureuser@$CONNECTOR_IP
   
   # From connector, verify Fortinet is reachable
   ping 10.10.1.4  # Fortinet firewall IP
   
   # Check route table (verify Fortinet is default gateway)
   ip route
   
   # Should show something like:
   # default via 10.10.1.4 dev eth0
   ```

5. **Test connectivity to ZPA** (optional):
   ```bash
   # From connector VM, verify outbound connectivity
   curl -v https://api.private.zscaler.com/health
   
   # Should succeed (200 response) if:
   # - Firewall allows outbound TCP 443 to Zscaler IPs
   # - Credentials are correct
   ```

---

## Understanding Your Infrastructure

### Resource Layout After Deployment

```
ESS-DEV Subscription
├── rg-network-essdev (EXISTING)
│   ├── ess-dev-vnet (EXISTING)
│   │   ├── app-subnet (EXISTING - where connectors deploy)
│   │   └── Fortinet firewall (EXISTING - routes all traffic)
│
└── rg-zpa-connectors-poc (NEW - created by Terraform)
    ├── zsac-ac-vm-0 (NEW - App Connector 1)
    ├── zsac-ac-vm-1 (NEW - App Connector 2)
    ├── NSG rules (NEW - for VM management)
    └── Network interfaces (NEW - 2 NICs, one per VM)
```

### Traffic Flow

1. **Outbound from App Connector**:
   - App Connector → Fortinet Firewall (10.10.1.4)
   - Firewall → Zscaler Cloud (validated by firewall policy)
   - Zscaler → Internet/Applications

2. **Inbound application access** (via ZPA policy):
   - User → ZPA Portal → Connector Group → Private App
   - Enforcement: Via ZPA access policies (not firewall)

---

## Cost Management

**Deployment cost**:
- 1 week POV: ~$40
- 2 weeks POV: ~$80
- Full month POV: ~$150-180

**Save money during testing**:
- Stop VMs (don't delete) when not testing:
  ```bash
  az vm deallocate -g rg-zpa-connectors-poc --ids \
    /subscriptions/ESS-DEV-ID/resourceGroups/rg-zpa-connectors-poc/providers/Microsoft.Compute/virtualMachines/zsac-ac-vm-0 \
    /subscriptions/ESS-DEV-ID/resourceGroups/rg-zpa-connectors-poc/providers/Microsoft.Compute/virtualMachines/zsac-ac-vm-1
  ```
  - Saves ~30% (only storage charges, no compute)

**Delete when POV ends**:
```bash
terraform destroy
# Confirm 'yes'
# Wait 2-5 minutes
```

---

## Troubleshooting

| Problem | Likely Cause | Solution |
|---------|-------------|----------|
| Connectors stuck "Pending" in ZPA | Firewall blocking outbound 443 | Check Fortinet policy; verify allow rule for ZPA API endpoints |
| "EncryptionAtHost not enabled" | Azure feature not registered | Set `encryption_at_host_enabled = false` in tfvars |
| NSG not allowing SSH | Default NSG too restrictive | Update NSG to allow TCP 22 from your IP/Fortinet subnet |
| Terraform can't find subnet | Wrong RG name or subnet name | Verify names in tfvars match exactly (case-sensitive) |
| Cannot SSH to connector | Firewall not allowing 22 from management subnet | Adjust Fortinet rules or NSG source IPs |

---

## Cleanup & Teardown

**To destroy all resources** (removes new RG and VMs, leaves existing VNet):

```bash
cd examples/ac

# Review what will be deleted
terraform plan -destroy

# Delete everything Terraform created
terraform destroy

# Confirm 'yes' when prompted
# Wait 2-5 minutes
```

**Result**:
- New resource group `rg-zpa-connectors-poc` deleted
- App Connector VMs deleted
- Network interfaces and NSGs deleted
- **Existing** VNet and subnet in `rg-network-essdev` remain untouched
- Terraform state files preserved locally (safe to delete if desired)
- App Connector Group remains in ZPA (can be manually deleted from portal)

---

## Critical Files

| Path | Purpose |
|---|---|
| `/examples/ac/` | Main deployment directory |
| `/examples/ac/terraform.tfvars` | Your configuration (EDIT THIS) |
| `/examples/ac/main.tf` | Terraform module composition |
| `/examples/ac/variables.tf` | Variable definitions |
| `/modules/terraform-zsac-acvm-azure/` | VM module (reference only) |
| `terraform.tfstate` | Deployment state (do NOT commit to Git) |
| `.zsacrc` | Credentials cache (gitignored) |
| `zsac-key.pem` | SSH private key (keep safe) |

---

## Success Criteria

✅ **POV is successful when:**
1. ✅ New resource group created in ESS-DEV subscription
2. ✅ 2 App Connector VMs deployed in East US region
3. ✅ Connectors placed in existing subnet with Fortinet routing
4. ✅ Both connectors show **"Connected"** in ZPA portal within 20 minutes
5. ✅ You can SSH to a connector using the generated key
6. ✅ Fortinet logs show outbound traffic to ZPA API endpoints
7. ✅ Ready for production planning

---

---

## Fallback: Manual GUI Deployment (If Terraform Fails)

**Purpose**: Deploy connectors manually via Azure Portal + CLI if Terraform encounters errors.

**When to use**: Terraform fails with provider issues, state corruption, or version conflicts.

### Prerequisites for Manual Deployment

Same as Terraform, plus:
- [ ] Azure Portal access (using Service Principal credentials or user account)
- [ ] Azure CLI installed and authenticated
- [ ] Manual deployment takes ~60-90 minutes (vs. 30-40 min Terraform)

### Step-by-Step Manual Deployment

#### A. Authenticate with Service Principal (Azure CLI)

```bash
# Login with Service Principal
az login --service-principal \
  -u "<CLIENT_ID>" \
  -p "<CLIENT_SECRET>" \
  --tenant "<TENANT_ID>"

# Set subscription context
az account set --subscription "ESS-DEV-SUBSCRIPTION-ID"

# Verify authentication
az account show
```

#### B. Create Resource Group

```bash
az group create \
  --name rg-zpa-connectors-poc \
  --location eastus
```

#### C. Create Network Security Group (NSG)

```bash
# Create NSG
az network nsg create \
  --name zsac-ac-nsg \
  --resource-group rg-zpa-connectors-poc \
  --location eastus

# Add NSG rules
# Allow SSH (if needed for management)
az network nsg rule create \
  --nsg-name zsac-ac-nsg \
  --resource-group rg-zpa-connectors-poc \
  --name allow-ssh \
  --priority 100 \
  --direction Inbound \
  --access Allow \
  --protocol Tcp \
  --source-address-prefixes "<YOUR_IP>/32" \
  --destination-port-ranges 22

# Allow outbound HTTPS to Zscaler
az network nsg rule create \
  --nsg-name zsac-ac-nsg \
  --resource-group rg-zpa-connectors-poc \
  --name allow-zpa-outbound \
  --priority 100 \
  --direction Outbound \
  --access Allow \
  --protocol Tcp \
  --source-address-prefixes "*" \
  --destination-port-ranges 443
```

#### D. Get Existing Subnet Information

```bash
# Get subnet ID
SUBNET_ID=$(az network vnet subnet show \
  --resource-group rg-network-essdev \
  --vnet-name ess-dev-vnet \
  --name app-subnet \
  --query id -o tsv)

echo "Subnet ID: $SUBNET_ID"
```

#### E. Create Network Interfaces

```bash
# Create NIC for VM 1
az network nic create \
  --name zsac-ac-nic-0 \
  --resource-group rg-zpa-connectors-poc \
  --location eastus \
  --subnet $SUBNET_ID \
  --network-security-group zsac-ac-nsg \
  --private-ip-address 10.10.20.10

# Create NIC for VM 2
az network nic create \
  --name zsac-ac-nic-1 \
  --resource-group rg-zpa-connectors-poc \
  --location eastus \
  --subnet $SUBNET_ID \
  --network-security-group zsac-ac-nsg \
  --private-ip-address 10.10.20.11

# Get NIC IDs
NIC_0=$(az network nic show \
  --resource-group rg-zpa-connectors-poc \
  --name zsac-ac-nic-0 \
  --query id -o tsv)

NIC_1=$(az network nic show \
  --resource-group rg-zpa-connectors-poc \
  --name zsac-ac-nic-1 \
  --query id -o tsv)
```

#### F. Generate SSH Key Pair

```bash
# Generate SSH key (save to secure location)
ssh-keygen -t rsa -b 4096 -f ~/.ssh/zsac-key -N ""

# Copy public key
cat ~/.ssh/zsac-key.pub
```

#### G. Create Virtual Machines

```bash
# VM 1
az vm create \
  --resource-group rg-zpa-connectors-poc \
  --name zsac-ac-vm-0 \
  --nics $NIC_0 \
  --size Standard_D4s_v5 \
  --image UbuntuLTS \
  --os-disk-name zsac-ac-vm-0-disk \
  --os-disk-size-gb 128 \
  --os-disk-caching ReadWrite \
  --ssh-key-values ~/.ssh/zsac-key.pub \
  --custom-data user-data-script.sh \
  --no-wait

# VM 2
az vm create \
  --resource-group rg-zpa-connectors-poc \
  --name zsac-ac-vm-1 \
  --nics $NIC_1 \
  --size Standard_D4s_v5 \
  --image UbuntuLTS \
  --os-disk-name zsac-ac-vm-1-disk \
  --os-disk-size-gb 128 \
  --os-disk-caching ReadWrite \
  --ssh-key-values ~/.ssh/zsac-key.pub \
  --custom-data user-data-script.sh \
  --no-wait

# Wait for VMs to complete
az vm wait --created -g rg-zpa-connectors-poc --ids \
  "$(az vm show -d -g rg-zpa-connectors-poc -n zsac-ac-vm-0 --query id -o tsv)" \
  "$(az vm show -d -g rg-zpa-connectors-poc -n zsac-ac-vm-1 --query id -o tsv)"
```

#### H. Prepare App Connector User Data Script

**Create file**: `user-data-script.sh`

```bash
#!/bin/bash
set -e

# Update system
apt-get update
apt-get upgrade -y

# Install App Connector (download from Zscaler portal or use published image)
# This typically involves:
# 1. Download App Connector package
# 2. Install Zscaler agents
# 3. Configure connectors

# Example (actual steps vary by Zscaler version):
wget https://zscaler-app-connector-repo.s3.amazonaws.com/zpa-connector-latest.tar.gz
tar -xzf zpa-connector-latest.tar.gz
cd zpa-connector
./install.sh

# Configure with OAuth2 or Provisioning Key
# This is typically done via environment variables or config file
export ZPA_CLIENT_ID="your-zpa-credentials"
export ZPA_PROVISIONING_KEY="your-key"

# Start connector service
systemctl start zpa
systemctl enable zpa
```

#### I. Register App Connector Group in ZPA (Manual)

1. Log into ZPA Admin Portal
2. Navigate: **Administration** → **App Connectors** → **Connector Groups**
3. Click **Add Group**
4. Fill in:
   - **Group Name**: `ESS-DEV-AC-Group`
   - **Location**: San Jose, CA, USA
   - **Description**: POV - ESS-DEV
5. Click **Save**
6. Copy generated **Provisioning Key** (if using legacy auth)

#### J. Manually Configure Connectors

SSH into each VM and configure:

```bash
# SSH into VM 1
ssh -i ~/.ssh/zsac-key azureuser@10.10.20.10

# On the VM, configure the connector
# Steps depend on your ZPA version and authentication method

# For OAuth2:
sudo tee /etc/zpa/connector-config.conf << EOF
ZPA_CLIENT_ID=<your-client-id>
ZPA_CLIENT_SECRET=<your-secret>
ZPA_CUSTOMER_ID=<your-customer-id>
ZPA_PROVISIONING_KEY=<auto-generated-key>
EOF

# Restart connector
sudo systemctl restart zpa
```

### Limitations of Manual Deployment

| Aspect | Terraform | Manual GUI |
|--------|-----------|-----------|
| **Time** | ~30-40 min | ~60-90 min |
| **Reproducibility** | Repeatable, versioned | Manual, hard to repeat exactly |
| **IaC** | Full infrastructure-as-code | No versioning |
| **Maintenance** | Easy updates | Manual updates needed |
| **Destruction** | One command | Manual deletion needed |
| **Error Recovery** | State file can help debug | Harder to track what was done |

### GUI Portal Deployment (Alternative)

**If you want to use only Azure Portal GUI** (no CLI):

1. **Create Resource Group**: Portal → Resource Groups → Create
2. **Create NSG**: Portal → Network Security Groups → Create
3. **Create VMs**: Portal → Virtual Machines → Create
   - Select: RHEL 9 or Ubuntu image
   - Size: Standard_D4s_v5
   - Networking: Existing VNet/Subnet, your NSG
   - Disks: 128GB managed disk
   - Admin: SSH key (paste `ssh-keyed-values` output)
4. **Connect via Portal Bastion** (if Bastion exists) or SSH after VM boots

### When Manual Deployment Makes Sense

- **Terraform errors**: Provider version conflicts, state corruption
- **One-off POC**: Not repeating deployment; don't need IaC
- **Learning**: Understanding what resources are created
- **Troubleshooting**: Isolating whether issue is Terraform or Azure

### Service Principal Access to Portal

**Your Service Principal credentials CAN be used to log into Azure Portal:**

```bash
# Option 1: Via Azure CLI (recommended for automation)
az login --service-principal \
  -u "<CLIENT_ID>" \
  -p "<CLIENT_SECRET>" \
  --tenant "<TENANT_ID>"

# Then use Portal web UI (already authenticated in this session)
```

**However**:
- Service Principal logins to Portal UI are supported but not recommended (portal is designed for user logins)
- Better to log in as a user, then CLI commands run under Service Principal
- For Portal GUI work, use your user account + Service Principal for automation

---

## Next Steps

1. **Try Terraform first** → Simpler, faster, more reproducible
2. **If Terraform fails** → Check error message; often fixable with tfvars tweaks
3. **Last resort: Manual** → Use Azure CLI or Portal GUI as documented above
4. **POV successful** → Begin planning production deployment
5. **Need more testing** → Keep resources running (only ~$150/month)
6. **Add applications** → Create ZPA access policies in admin portal
7. **Scale deployment** → Use same `ac` example for additional connectors
8. **Ready for production** → Plan migration to prod subscription with increased monitoring

