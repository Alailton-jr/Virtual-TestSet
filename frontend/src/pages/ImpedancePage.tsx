import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card'

export default function ImpedancePage() {
  return (
    <div className="space-y-6">
      <div>
        <h1 className="text-3xl font-bold tracking-tight">Impedance Injection</h1>
        <p className="text-muted-foreground">
          Inject fault impedance for protection testing
        </p>
      </div>

      <Card>
        <CardHeader>
          <CardTitle>Fault Configuration</CardTitle>
          <CardDescription>
            Configure fault type, impedance (R+jX), and source parameters
          </CardDescription>
        </CardHeader>
        <CardContent>
          <p className="text-sm text-muted-foreground">
            Impedance injection controls coming soon...
          </p>
        </CardContent>
      </Card>
    </div>
  )
}
